#include "thread_info.h"
#include <concore2full/detail/atomic_wait.h>
#include <concore2full/detail/callcc.h>
#include <concore2full/global_thread_pool.h>

#include <cassert>
#include <mutex>
#include <vector>

namespace concore2full::detail {

namespace {

/*
Switching between threads involves the following steps:
1. (originator) Start the switch process, blocking the two threads from doing other switches;
2. (originator) Publish its continuation point to the target thread; wakes up the target thread
3. (target) while checking inversion, read the stored continuation point and publish its
continuation point
4. (target) tries to continue the control flow of the originator thread
5. (originator) waits a continuation from the target thread
6. (originator) mark the switch as completed, unblocking the two threads
7. (originator) continues the control flow of the target thread
8. (target) continue the control flow of the originator thread

Dependencies:

originator: 1 -> 2     /-> 5 -> 6 -> 7 ...
target:           \-> 3 -> 4 ->  \-> 8 ...

Data access across threads:
- target thread:
  - mostly writes/reads to/from its thread data
  - writes target->switching_to_ for the originator thread
    - this happens at point 3, while originator waits for this value (point 5)

- originator thread:
  - initiates the switch (1) under mutex protection (reading & writing
target->is_currently_switching_)
    - target thread is alive, as we are the ones that should terminate the target thread
  - terminates the switch (6)
    - target thread is alive, waiting for us to terminate the switch
  - reads target->switching_to_ (point 5) while target writes it (point 3)
    - target thread is not going our of scope, as we are ending the switch
  - writes target->switching_to_
    - target thread is alive, as we are the ones that should terminate the target thread
    - we are the only one that started the switch process, and no-one else can write to this field
  - writes target->should_switch_with_
    - target thread is alive, as we are the ones that should terminate the target thread
    - we are the only one that started the switch process, and no-one else can write to this field
  - reads & uses target->sleeping_counter_
    - target thread is alive, as we are the ones that should terminate the target thread
*/

//! The data associated with each thread.
thread_local thread_info tls_thread_info;

//! Global mutex to track dependencies between threads when requesting thread switch.
static std::mutex g_thread_dependency_bottleneck;

//! All threads that we've seen.
static std::vector<thread_info*> g_threads;

//! Global mutex to protect access to `g_threads`
static std::mutex g_threads_bottleneck;

//! Quick way to get a number from a thread ID. Used in profiling.
// uint64_t thread_id_number(std::thread::id id) { return *reinterpret_cast<uint64_t*>(&id); }

//! Add a thread to our list of threads.
void add_thread(thread_info* info) {
  std::lock_guard<std::mutex> lock{g_threads_bottleneck};
  g_threads.push_back(info);
}

//! Remove a thread to our list of threads.
void remove_thread(thread_info* info) {
  std::lock_guard<std::mutex> lock{g_threads_bottleneck};
  auto it = std::find(g_threads.begin(), g_threads.end(), info);
  if (it != g_threads.end())
    g_threads.erase(it);
}

//! Find the thread info for a given thread ID; returns nullptr if one cannot be found.
thread_info* find_thread(std::thread::id id) {
  std::lock_guard<std::mutex> lock{g_threads_bottleneck};
  auto it = std::find_if(g_threads.begin(), g_threads.end(),
                         [id](auto* info) { return info->thread_id_ == id; });
  return it == g_threads.end() ? nullptr : *it;
}

//! Try starting the switch between `current` and `target` threads.
//! If the switch succeeds, `target` is instructed to continue with `target_continuation`.
//! @return `true` if the switch was started; `false` if the switch could not be started.
bool try_start_switch(thread_info* current, thread_info* target) {
  // We use a lock here to protect against accessing it from multiple threads, in different orders.
  std::lock_guard<std::mutex> lock{g_thread_dependency_bottleneck};
  if (!current->is_currently_switching_.load(std::memory_order_relaxed) &&
      !target->is_currently_switching_.load(std::memory_order_relaxed)) {
    // Mark the start of the switch.
    current->is_currently_switching_.store(true, std::memory_order_relaxed);
    target->is_currently_switching_.store(true, std::memory_order_relaxed);
    return true;
  } else
    return false;
}

//! End the switch process.
void end_switch(thread_info* originator, thread_info* target) {
  std::lock_guard<std::mutex> lock{g_thread_dependency_bottleneck};
  originator->switching_to_.store(nullptr, std::memory_order_relaxed);
  originator->is_currently_switching_.store(false, std::memory_order_relaxed);
  target->is_currently_switching_.store(false, std::memory_order_relaxed);
  target->should_switch_with_.store(nullptr, std::memory_order_release);
}

//! Repeatedly try to start the switch process, until it succeeds.
//! Returns `true` when the switch is started, and `false` if we are on the desired thread.
bool do_start_switch(thread_info* target) {
  while (true) {
    if (try_start_switch(&get_current_thread_info(), target))
      return true;
    // We cannot switch at this moment.
    // Check if another thread requested a switch from us.
    check_for_thread_switch();

    // This function may return on a different thread; check if we are on the desired thread.
    if (&get_current_thread_info() == target)
      return false;

    // If we still need to switch, introduce a small pause, so that we don't consume the CPU while
    // spinning.
    std::this_thread::yield();
  }
}

//! Wait for continuation corresponding to `current` to be published (by another thread).
//! @return The continuation that was published.
continuation_t wait_for_continuation(thread_info* thread) {
  continuation_t r;
  wait_with_backoff([thread, &r]() {
    r = thread->switching_to_.load(std::memory_order_relaxed);
    return r != nullptr;
  });
  return r;
}

void requested_switch_with(thread_info* target) {
  profiling::zone zone{CURRENT_LOCATION()};
  // The switch data will be stored on the first thread.
  (void)detail::callcc([target](detail::continuation_t c) -> detail::continuation_t {
    auto* current = &get_current_thread_info();
    assert(current != target);

    // Get the continuation for us.
    auto next_for_us = current->switching_to_.load(std::memory_order_relaxed);
    // Sync: this is published before the originator thread requests the switch that gets us here.
    current->switching_to_.store(nullptr, std::memory_order_relaxed);
    // Sync: no other store/load operations are dependent on this store. We will sync later with a
    // mutex when ending the switching.

    // Set the continuation for the originator thread.
    target->switching_to_.store(c, std::memory_order_relaxed);
    // Sync: No previous store operations that need to be released with this store.

    return next_for_us;
  });
  // The originating thread will continue this control flow.
  assert(target == &get_current_thread_info());
}

} // namespace

thread_info::thread_info() {
  // profiling::zone_instant zone{CURRENT_LOCATION()};
  // zone.set_param("cur_thread,x", this);
  // zone.add_flow(this);
  thread_id_ = std::this_thread::get_id();
  add_thread(this);
}
thread_info::~thread_info() {
  // profiling::zone_instant zone{CURRENT_LOCATION()};
  // zone.set_param("cur_thread,x", this);
  // zone.add_flow(this);
  remove_thread(this);
}

thread_info& get_current_thread_info() {
  thread_info* result = &tls_thread_info;

  // Double-check that this is the right thread.
  // It seems that thread_local doesn't always work when in the same function we call this function,
  // while the thread is changing.
  if (result->thread_id_ != std::this_thread::get_id()) {
    // profiling::zone_instant z1{CURRENT_LOCATION_N("thread_id mismatch")};
    // z1.set_param("cur_thread,x", result);
    // z1.set_param("thread-id", thread_id_number(result->thread_id_));
    // z1.add_flow(result);
    // We are on a different thread.
    result = find_thread(std::this_thread::get_id());
    if (!result) {
      // This is a new thread.
      result = &tls_thread_info;
      result->thread_id_ = std::this_thread::get_id();
      add_thread(result);
    }
  }
  // profiling::zone_instant zone{CURRENT_LOCATION()};
  // zone.set_param("cur_thread,x", result);
  // zone.add_flow(result);
  // zone.set_param("thread-id", thread_id_number(result->thread_id_));

  return *result;
}

void switch_to(thread_info* target) {
  profiling::zone zone{CURRENT_LOCATION()};

  if (!do_start_switch(target)) {
    // We are on the desired thread.
    return;
  }

  thread_info* current = &get_current_thread_info();
  if (current == target) {
    return;
  }

  std::atomic<bool> done{false};

  // If we are here, we are just starting the switch.
  (void)detail::callcc(
      [current, target, &done](detail::continuation_t c) -> detail::continuation_t {
        // Start waking up the other thread; make sure the continuation is set.
        target->switching_to_.store(c, std::memory_order_relaxed);
        // Sync: no writes need to be published with this store.
        target->should_switch_with_.store(current, std::memory_order_release);
        // Sync: ensure target->switching_to_ is visible before this store.

        // Note: after this point, the other thread sees the "switch request" and can immediately
        // continue on the current thread.
        // Note: the other thread cannot exit the scope of `switch_to` until the switch is done.

        // Make sure the other thread is woken up.
        wake_up(*target);

        // Note: after this point, the other thread can finish the "switch request" and immediately
        // continue on the current thread. This thread might end very soon.

        // Wait for `target` to publish its continuation point, so that we can continue there.
        auto next_continuation = wait_for_continuation(current);

        // Mark the end of the switch process.
        end_switch(current, target);
        // Let the `target` thread continue with the continuation point.
        done.store(true, std::memory_order_relaxed);
        // After this point, the stack of `switch_to`, and the entire thread calling it might be
        // destroyed.

        // We jump to the continuation point in `requested_switch_with`.
        return next_continuation;
      });
  // We resume here on the original thread.
  assert(target == &get_current_thread_info());

  // Don't exit (possibly destroying the current thread) before the other thread has finished the
  // switching process.
  wait_with_backoff([&done]() { return done.load(std::memory_order_relaxed); });
}

void check_for_thread_switch() {
  // Check if some other thread requested us to switch.
  auto& cur_thread = detail::get_current_thread_info();
  auto* originator = cur_thread.should_switch_with_.load(std::memory_order_acquire);
  // Sync: ensure we see the value of switching_to_.
  if (originator) {
    requested_switch_with(originator);
  }
}

uint32_t prepare_sleep(thread_info& thread) {
  return thread.sleeping_counter_.load(std::memory_order_acquire);
  // Sync: treat this sleep as an acquire barrier, to help with synchronization in the outside code.
}

void sleep(thread_info& thread, uint32_t sleep_id) {
  thread.sleeping_counter_.wait(sleep_id, std::memory_order_acquire);
  // Sync: treat this sleep as an acquire barrier.
}

void wake_up(thread_info& thread) {
  thread.sleeping_counter_.fetch_add(1, std::memory_order_release);
  thread.sleeping_counter_.notify_one();
  // Sync: treat this wake-up as a release barrier.
}

} // namespace concore2full::detail
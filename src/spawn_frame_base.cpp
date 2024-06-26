#include "concore2full/detail/spawn_frame_base.h"
#include "concore2full/detail/atomic_wait.h"
#include "concore2full/global_thread_pool.h"
#include "concore2full/profiling.h"

#include <chrono>
#include <cstring>

namespace {

using concore2full::detail::callcc;
using concore2full::detail::continuation_t;
using concore2full::detail::spawn_frame_base;

/*
Valid transitions:
ss_initial_state -> ss_async_started --> ss_async_finished
                                     \-> ss_main_finishing -> ss_main_finished
*/
enum sync_state_values {
  ss_initial_state = 0,
  ss_async_started,
  ss_async_finished,
  ss_main_finishing,
  ss_main_finished,
};

} // namespace

void spawn_frame_base::spawn(concore2full_spawn_function_t f) {
  task_.task_function_ = &execute_spawn_task;
  task_.next_ = nullptr;
  sync_state_ = ss_initial_state;
  user_function_ = f;
  concore2full::global_thread_pool().enqueue(&task_);
}
void spawn_frame_base::await() {
  // If the async work hasn't started yet, check if we can execute it here directly.
  if (atomic_load_explicit(&sync_state_, std::memory_order_acquire) == ss_initial_state) {
    if (concore2full::global_thread_pool().extract_task(&task_)) {
      concore2full::profiling::zone z{CURRENT_LOCATION_N("execute inplace")};
      // We've extracted the task from the queue; execute it here directly.
      user_function_(to_interface());
      // We are done.
      return;
    }
    // If we are here, the task was already started by the thread pool.
    // Wait for it to store the continuation object.
    concore2full::detail::atomic_wait(sync_state_, [](int v) { return v >= ss_async_started; });
  }

  uint32_t expected{ss_async_started};
  if (atomic_compare_exchange_strong(&sync_state_, &expected, ss_main_finishing)) {
    // The main thread is first to finish; we need to start switching threads.
    auto c = callcc([this](continuation_t await_cc) -> continuation_t {
      originator_ = await_cc;
      // We are done "finishing".
      atomic_store_explicit(&sync_state_, ss_main_finished, std::memory_order_release);
      // Complete the thread switching.
      return secondary_thread_;
    });
    (void)c;
  } else {
    // The async thread finished; we can continue directly, no need to switch threads.
  }
  // This point will be executed by the thread that finishes last.
}

//! Called when the async work is finished, to see if we need a thread switch.
continuation_t spawn_frame_base::on_async_complete(continuation_t c) {
  uint32_t expected{ss_async_started};
  if (atomic_compare_exchange_strong(&sync_state_, &expected, ss_async_finished)) {
    // We are first to arrive at completion.
    // We won't need any thread switch, so we can safely exit.
    // Return the original continuation.
    return c;
  } else {
    // We are the last to arrive at completion, and we need a thread switch.

    // If the main thread is currently finishing, wait for it to finish.
    // We need the main thread to properly call `originator_start`.
    concore2full::detail::atomic_wait(sync_state_, [](int v) { return v == ss_main_finished; });

    // Finish the thread switch.
    return originator_;
  }
}

//! The task function that executes the async work.
void spawn_frame_base::execute_spawn_task(concore2full_task* task, int) noexcept {
  auto self = (spawn_frame_base*)((char*)task - offsetof(spawn_frame_base, task_));
  (void)callcc([self](continuation_t thread_cont) -> continuation_t {
    // Assume there will be a thread switch and store required objects.
    self->secondary_thread_ = thread_cont;
    // Signal the fact that we have started (and the continuation is properly stored).
    atomic_store_explicit(&self->sync_state_, ss_async_started, std::memory_order_release);
    // Actually execute the given work.
    self->user_function_(self->to_interface());
    // Complete the async processing.
    return self->on_async_complete(thread_cont);
  });
}

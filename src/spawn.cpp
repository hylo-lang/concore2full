#include "concore2full/spawn.h"
#include "concore2full/global_thread_pool.h"
#include "concore2full/profiling.h"

#include <chrono>
#include <cstring>

namespace {

using concore2full::detail::callcc;
using concore2full::detail::continuation_t;

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

//! Wait until the given function returns true.
template <typename F> void wait_with_backoff(F&& f) {
  constexpr int polling_count = 64;
  uint32_t i = 0;
  while (true) {
    if (f())
      return;
    // Do some polling.
    if (i++ < polling_count)
      continue;

    // Yield the control for this OS thread.
    std::this_thread::yield();
    i = 0;
  }
}

//! Wait until the given function, applied to the value read from `a`, returns true.
template <typename F> void atomic_wait(CONCORE2FULL_ATOMIC(int) & a, F&& f) {
  wait_with_backoff([&a, f = std::forward<F>(f)]() {
    int v = atomic_load_explicit(&a, std::memory_order_acquire);
    return f(v);
  });
}

//! Called when the async work is finished, to see if we need a thread switch.
continuation_t on_async_complete(concore2full_spawn_frame* frame, continuation_t c) {
  int expected{ss_async_started};
  if (atomic_compare_exchange_strong(&frame->sync_state_, &expected, ss_async_finished)) {
    // We are first to arrive at completion.
    // We won't need any thread switch, so we can safely exit.
    // Return the original continuation.
    return c;
  } else {
    // We are the last to arrive at completion, and we need a thread switch.

    // If the main thread is currently finishing, wait for it to finish.
    // We need the main thread to properly call `originator_start`.
    atomic_wait(frame->sync_state_, [](int v) { return v == ss_main_finished; });

    // Finish the thread switch.
    return concore2full_exchange_thread_with(&frame->switch_data_.originator_);
  }
}

//! The task function that executes the async work.
void execute_spawn_task(concore2full_task* task, int) noexcept {
  auto frame = (concore2full_spawn_frame*)((char*)task - offsetof(concore2full_spawn_frame, task_));
  (void)callcc([frame](continuation_t thread_cont) -> continuation_t {
    // Assume there will be a thread switch and store required objects.
    concore2full_store_thread_data(&frame->switch_data_.target_, thread_cont);
    // Signal the fact that we have started (and the continuation is properly stored).
    atomic_store_explicit(&frame->sync_state_, ss_async_started, std::memory_order_release);
    // Actually execute the given work.
    frame->user_function_(frame);
    // Complete the async processing.
    return on_async_complete(frame, thread_cont);
  });
}

} // namespace

extern "C" void concore2full_spawn(struct concore2full_spawn_frame* frame,
                                   concore2full_spawn_function_t f) {
  frame->task_.task_function_ = &execute_spawn_task;
  frame->task_.next_ = nullptr;
  frame->sync_state_ = ss_initial_state;
  memset(&frame->switch_data_, 0, sizeof(concore2full_thread_switch_data));
  frame->user_function_ = f;
  concore2full::global_thread_pool().enqueue(&frame->task_);
}

extern "C" void concore2full_await(struct concore2full_spawn_frame* frame) {
  // Ensure that we started the async work (and the continuation is set).
  atomic_wait(frame->sync_state_, [](int v) { return v >= ss_async_started; });
  // Now, the possible states are: ss_async_started, ss_async_finished

  int expected{ss_async_started};
  if (atomic_compare_exchange_strong(&frame->sync_state_, &expected, ss_main_finishing)) {
    // The main thread is first to finish; we need to start switching threads.
    auto c = callcc([frame](continuation_t await_cc) -> continuation_t {
      concore2full_store_thread_data(&frame->switch_data_.originator_, await_cc);
      // We are done "finishing".
      atomic_store_explicit(&frame->sync_state_, ss_main_finished, std::memory_order_release);
      // Complete the thread switching.
      return concore2full_exchange_thread_with(&frame->switch_data_.target_);
    });
    (void)c;
  } else {
    // The async thread finished; we can continue directly, no need to switch threads.
  }
  // This point will be executed by the thread that finishes last.
}

extern "C" void concore2full_spawn2(struct concore2full_spawn_frame* frame,
                                    concore2full_spawn_function_t* f) {
  concore2full_spawn(frame, *f);
}

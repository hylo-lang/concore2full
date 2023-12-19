#include "concore2full/spawn.h"
#include "concore2full/profiling.h"

#include <chrono>
#include <stdatomic.h>

namespace concore2full::detail {
void execute_spawn_task(concore2full_task* frame, int) noexcept;
}

enum sync_state_values {
  both_working,
  main_finishing,
  main_finished,
  async_finished,
};

extern "C" void concore2full_initialize(struct concore2full_spawn_frame* frame,
                                        concore2full_spawn_function_t user_function) {
  memset(frame, 0, sizeof(concore2full_spawn_frame));
  frame->task_.task_function_ = &concore2full::detail::execute_spawn_task;
  frame->sync_state_ = both_working;
  frame->user_function_ = user_function;
}

namespace concore2full::detail {

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

void wait(_Atomic(int)& a, int old, int memory_order) {
  wait_with_backoff(
      [&a, old, memory_order]() { return atomic_load_explicit(&a, memory_order) != old; });
}

detail::continuation_t on_async_complete(concore2full_spawn_frame* frame,
                                         detail::continuation_t c) {
  int expected{both_working};
  if (atomic_compare_exchange_strong(&frame->sync_state_, &expected, async_finished)) {
    // We are first to arrive at completion.
    // We won't need any thread switch, so we can safely exit.
    // Return the original continuation.
    return c;
  } else {
    // We are the last to arrive at completion, and we need a thread switch.

    // If the main thread is currently finishing, wait for it to finish.
    // We need the main thread to properly call `originator_start`.
    wait(frame->sync_state_, main_finishing, memory_order_acquire);

    // Finish the thread switch.
    return concore2full_exchange_thread_with(&frame->switch_data_.originator_);
  }
}

void on_main_complete(concore2full_spawn_frame* frame) {
  int expected{both_working};
  if (atomic_compare_exchange_strong(&frame->sync_state_, &expected, main_finishing)) {
    // The main thread is first to finish; we need to start switching threads.
    auto c = detail::callcc([frame](detail::continuation_t await_cc) -> detail::continuation_t {
      concore2full_store_thread_data(&frame->switch_data_.originator_, await_cc);
      // We are done "finishing".
      atomic_store_explicit(&frame->sync_state_, main_finished, memory_order_release);
      // Ensure that we started the async work (and the continuation is set).
      wait(frame->async_started_, 0, memory_order_acquire);
      // Complete the thread switching.
      return concore2full_exchange_thread_with(&frame->switch_data_.target_);
    });
    (void)c;
  } else {
    // The async thread finished; we can continue directly, no need to switch threads.
  }
  // This point will be executed by the thread that finishes last.
}

void execute_spawn_task(concore2full_task* task, int) noexcept {
  auto frame = (concore2full_spawn_frame*)((char*)task - offsetof(concore2full_spawn_frame, task_));
#if USE_TRACY
  profiling::duplicate_zones_stack scoped_zones_stack{zones_};
#endif
  (void)detail::callcc([frame](detail::continuation_t thread_cont) -> detail::continuation_t {
    // Assume there will be a thread switch and store required objects.
    concore2full_store_thread_data(&frame->switch_data_.target_, thread_cont);
    // Signal the fact that we have started (and the continuation is properly stored).
    atomic_store_explicit(&frame->async_started_, 1, memory_order_release);
    // Actually execute the given work.
    frame->user_function_(frame);
    // Complete the async processing.
    return on_async_complete(frame, thread_cont);
  });
}

} // namespace concore2full::detail
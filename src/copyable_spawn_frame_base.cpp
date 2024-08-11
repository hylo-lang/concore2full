#include "concore2full/detail/copyable_spawn_frame_base.h"
#include "concore2full/detail/atomic_wait.h"
#include "concore2full/global_thread_pool.h"
#include "concore2full/profiling.h"

#include <chrono>
#include <cstring>

namespace {

using concore2full::detail::callcc;
using concore2full::detail::continuation_t;
using concore2full::detail::copyable_spawn_frame_base;

/*
Valid transitions:
ss_initial_state -> ss_async_started --> ss_async_finished
                                     \-> ss_main_finishing -> ss_main_finished ->
ss_async_finished_after_main
*/
enum sync_state_values {
  ss_initial_state = 0,
  ss_async_started,
  ss_async_finished,
  ss_main_finishing,
  ss_main_finished,
  ss_async_finished_after_main,
};

} // namespace

void copyable_spawn_frame_base::spawn(concore2full_spawn_function_t f) {
  sync_state_.set_name("sync_state");
  task_.task_function_ = &execute_spawn_task;
  task_.next_ = nullptr;
  sync_state_ = ss_initial_state;
  user_function_ = f;
  concore2full::global_thread_pool().enqueue(&task_);
}
void copyable_spawn_frame_base::await() {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  // If the async work hasn't started yet, check if we can execute it here directly.
  // if (sync_state_.load(std::memory_order_acquire) == ss_initial_state) {
  //   if (concore2full::global_thread_pool().extract_task(&task_)) {
  //     concore2full::profiling::zone z{CURRENT_LOCATION_N("execute inplace")};
  //     // We've extracted the task from the queue; execute it here directly.
  //     user_function_(to_interface());
  //     // We are done.
  //     return;

  //     // TODO: what about the followup awaits?
  //   }
  //   // If we are here, the task was already started by the thread pool.
  //   // Wait for it to store the continuation object.
  //   concore2full::detail::atomic_wait(sync_state_, [](int v) { return v >= ss_async_started; });
  // }

  // TODO
  concore2full::detail::atomic_wait(sync_state_, [](int v) { return v >= ss_async_started; });

  uint32_t expected{ss_async_started};
  if (sync_state_.compare_exchange_strong(expected, ss_main_finishing)) {
    // We are the first to finish; we need to start switching threads.
    auto c = callcc([this](continuation_t await_cc) -> continuation_t {
      first_await_ = await_cc;
      // We are done "finishing".
      sync_state_.store(ss_main_finished, std::memory_order_release);
      // Complete the thread switching.
      return secondary_thread_;
    });
    (void)c;
  } else {
    // We are not the first to arrive here; either the async work finished, or another thread
    // reached the await point before us.
    if (expected == ss_async_finished) {
      // The async thread finished; we can continue directly, no need to switch threads.
      return;
    } else {
      // There is another await that arrived before us; but we still need to wait for the async work
      // to finish
      concore2full::profiling::zone zone{CURRENT_LOCATION_N("waiting on both")};

      // If the async work has already finished, complete directly.
      auto state = sync_state_.load(std::memory_order_acquire);
      if (state == ss_async_finished || state == ss_async_finished_after_main) {
        return;
      }

      // Suspend the current thread; let the worker wake us up,
      suspend(suspend_token_);
    }
  }
}

//! Called when the async work is finished, to see if we need a thread switch.
continuation_t copyable_spawn_frame_base::on_async_complete(continuation_t c) {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};

  uint32_t expected{ss_async_started};
  if (sync_state_.compare_exchange_strong(expected, ss_async_finished)) {
    // We are first to arrive at completion.
    // We won't need any thread switch, so we can safely exit.
    // Return the original continuation.
    return c;
  } else {
    // There is at least one `await` that arrived before us; we need a thread swtich.

    // If the await thread is currently finishing, wait for it to finish.
    // We need the main thread to properly set `first_await_` continuation.
    concore2full::detail::atomic_wait(sync_state_, [](int v) { return v == ss_main_finished; });

    // Tell the world that the computation has finished.
    sync_state_.store(ss_async_finished_after_main, std::memory_order_release);

    // Notify all the waiting futures.
    suspend_token_.notify();

    // Finish the thread switch.
    return first_await_;
  }
}

//! The task function that executes the async work.
void copyable_spawn_frame_base::execute_spawn_task(concore2full_task* task, int) noexcept {
  auto self =
      (copyable_spawn_frame_base*)((char*)task - offsetof(copyable_spawn_frame_base, task_));
  (void)callcc([self](continuation_t thread_cont) -> continuation_t {
    // Assume there will be a thread switch and store required objects.
    self->secondary_thread_ = thread_cont;
    // Signal the fact that we have started (and the continuation is properly stored).
    self->sync_state_.store(ss_async_started, std::memory_order_release);
    // Actually execute the given work.
    self->user_function_(self->to_interface());
    // Complete the async processing.
    return self->on_async_complete(thread_cont);
  });
}

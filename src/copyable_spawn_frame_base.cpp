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
*/
enum sync_state_values {
  ss_initial_state = 0,
  ss_async_started,
  ss_async_finished,
};

} // namespace

void copyable_spawn_frame_base::spawn(concore2full_spawn_function_t f) {
  task_.task_function_ = &execute_spawn_task;
  task_.next_ = nullptr;
  sync_state_ = ss_initial_state;
  user_function_ = f;
  concore2full::global_thread_pool().enqueue(&task_);
}
void copyable_spawn_frame_base::await() {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  // If the async work has already finished, complete directly.
  if (sync_state_.load(std::memory_order_acquire) == ss_async_finished) {
    return;
  }

  // Suspend the current thread; let the worker wake us up,
  suspend(suspend_token_);
}

//! Called when the async work is finished, to see if we need a thread switch.
continuation_t copyable_spawn_frame_base::on_async_complete(continuation_t c) {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};

  // Tell the world that the computation has finished.
  sync_state_.store(ss_async_finished, std::memory_order_release);

  // Notify all the waiting futures.
  suspend_token_.notify();
  // TODO: the `this` object might become invalid if all the futures are destroyed.
  return c;
}

//! The task function that executes the async work.
void copyable_spawn_frame_base::execute_spawn_task(concore2full_task* task, int) noexcept {
  auto self =
      (copyable_spawn_frame_base*)((char*)task - offsetof(copyable_spawn_frame_base, task_));
  (void)callcc([self](continuation_t thread_cont) -> continuation_t {
    // Assume there will be a thread switch and store required objects.
    // self->secondary_thread_ = thread_cont;
    // Signal the fact that we have started (and the continuation is properly stored).
    atomic_store_explicit(&self->sync_state_, ss_async_started, std::memory_order_release);
    // Actually execute the given work.
    self->user_function_(self->to_interface());
    // Complete the async processing.
    return self->on_async_complete(thread_cont);
  });
}

#include "concore2full/suspend.h"
#include "concore2full/detail/atomic_wait.h"
#include "concore2full/detail/callcc.h"
#include "concore2full/global_thread_pool.h"

namespace concore2full {

namespace detail {
struct quick_resume_task : concore2full_task {
  explicit quick_resume_task(continuation_t c) : cont_(c) { task_function_ = &execute; }

  static void execute(struct concore2full_task* task, int worker_index) {
    auto* self = static_cast<quick_resume_task*>(task);
    callcc([self](continuation_t c) -> continuation_t {
      auto next = self->cont_;
      // Store the continuation after the task execution.
      self->after_execute_.store(c, std::memory_order_release);
      // After this store, the `self` object can be destroyed.
      // Jump to the point we want to resume.
      return next;
    });
  }

  continuation_t cont_;
  std::atomic<continuation_t> after_execute_{nullptr};
};
} // namespace detail

void suspend_token::notify() { stop_source_.request_stop(); }

void suspend(suspend_token& token) {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  if (token.stop_source_.stop_requested())
    return;
  global_thread_pool().offer_help_until(token.stop_source_.get_token());
}

void suspend_quick_resume(suspend_token& token) {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  auto stop_token = token.stop_source_.get_token();
  (void)detail::callcc([stop_token](
                           detail::continuation_t after_suspend) -> detail::continuation_t {
    // If we are already stopped, return immediately.
    if (stop_token.stop_requested())
      return after_suspend;

    detail::quick_resume_task task{after_suspend};
    enum { initial_state = 0, task_enqueuing, task_enqueued, task_not_needed };
    std::atomic<int> task_state{initial_state};

    // Register a stop callback that will spawn a new task to jump to the point after suspend.
    std::stop_callback cb{stop_token, [&task, &task_state]() {
                            int expected = initial_state;
                            if (task_state.compare_exchange_strong(expected, task_enqueuing,
                                                                   std::memory_order_release,
                                                                   std::memory_order_acquire)) {
                              concore2full::global_thread_pool().enqueue(&task);
                              task_state.store(task_enqueued, std::memory_order_release);
                            }
                          }};

    global_thread_pool().offer_help_until(stop_token);

    // Did the callback got a chance to run?
    int expected = initial_state;
    if (task_state.compare_exchange_strong(expected, task_not_needed, std::memory_order_acquire)) {
      // The callback didn't run; we can just return in the same stack.
      return after_suspend;
    }

    // When we wake up, try to steal the task.
    // First, wait for the task to be enqueued.
    concore2full::detail::atomic_wait(task_state, [](int s) { return s == task_enqueued; });
    if (concore2full::global_thread_pool().extract_task(&task)) {
      // All good; we can just return in the same stack.
      return after_suspend;
    } else {
      // If the task got the chance to run, then we need to resume at the point that the task
      // left it. Wait until the continuation is set
      concore2full::detail::atomic_wait(task.after_execute_,
                                        [](detail::continuation_t c) { return c != nullptr; });
      return task.after_execute_.load(std::memory_order_acquire);
    }
  });
}

} // namespace concore2full
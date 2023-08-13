#pragma once

#include <concepts>
#include <functional>

namespace concore2full::detail {

/// @brief Base class for task types.
///
/// This interface allows one to easily queue tasks to be executed. It provides a way to execute the
/// task, and also defines an intrusive list structure to able to chain tasks.
struct task_base {
  task_base* next_{nullptr};
  /// Execute the task.
  virtual void execute() noexcept = 0;
};

/// @brief Concrete task type that just calls a user-provided function.
/// @tparam Fn The type of the function wrapped by this task.
template <std::invocable Fn> struct fun_task : task_base {
  Fn f_;
  fun_task(Fn&& f) : f_(std::forward<Fn>(f)) {}

  void execute() noexcept override { std::invoke(f_); }
};

} // namespace concore2full::detail
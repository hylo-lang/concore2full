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

} // namespace concore2full::detail
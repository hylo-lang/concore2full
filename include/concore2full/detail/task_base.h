#pragma once

#include <concepts>
#include <functional>

namespace concore2full::detail {

struct task_base;

//! Type of a function that can be executed as a task.
using task_function_t = void (*)(task_base* task, int worker_index);

//! Represents a task that can be executed.
struct task_base {
  //! The function to be called to execute the task.
  task_function_t task_fptr_;
  //! Pointer to the next element in the list of task; implementation details.
  task_base* next_{nullptr};
};

} // namespace concore2full::detail
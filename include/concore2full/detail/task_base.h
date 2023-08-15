#pragma once

#include <concepts>
#include <functional>

namespace concore2full::detail {

/**
 * @brief Base class for task types.
 *
 * This interface allows one to easily queue tasks to be executed. It provides a way to execute the
 * task, and also defines an intrusive list structure to able to chain tasks.
 */
struct task_base {
  virtual ~task_base() = default;
  /**
   * @brief Execute the current task
   * @param index The index of the current worker thread.
   *
   * The index should be treated like a hint, with no strong guarantees attached to it. It may be
   * useful for debugging.
   */
  virtual void execute(int index) noexcept = 0;

  //! Pointer to the next element in the list of task; implementation details.
  task_base* next_{nullptr};
};

} // namespace concore2full::detail
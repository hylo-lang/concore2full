#pragma once

#include <concore2full/detail/task.h>

#include <cassert>

namespace concore2full::detail {

/**
 * @brief Simple task queue, that implements a FIFO.
 *
 * One can add elements to the back of the queue, and pop elements from the front.
 *
 * @sa task_base
 */
class task_queue {
  task_base* head_;
  task_base* tail_;

public:
  task_queue() = default;
  ~task_queue() = default;

  //! Checks if the queue is empty.
  [[nodiscard]] bool empty() const noexcept { return head_ == nullptr; }

  /**
   * @brief Add `elem` to the back of the queue.
   * @param elem The element to be added to the queue.
   */
  void push_back(task_base* elem) noexcept {
    assert(elem);
    if (!tail_)
      head_ = elem;
    else
      tail_->next_ = elem;
    tail_ = elem;
  }

  /**
   * @brief Pops the first element in the queue.
   * @return The first element in the queue.
   */
  [[nodiscard]] task_base* pop_front() noexcept {
    assert(!empty());
    auto old_head = head_;
    head_ = head_->next_;
    if (!head_)
      tail_ = nullptr;
    return old_head;
  }
};

} // namespace concore2full::detail
#pragma once

#include <concore2full/c/task.h>

#include <cassert>

namespace concore2full::detail {

/**
 * @brief Simple task queue, that implements a FIFO.
 *
 * One can add elements to the back of the queue, and pop elements from the front.
 *
 * @sa concore2full_task
 */
class task_queue {
  concore2full_task* head_;
  concore2full_task* tail_;

public:
  task_queue() = default;
  ~task_queue() = default;

  //! Checks if the queue is empty.
  [[nodiscard]] bool empty() const noexcept { return head_ == nullptr; }

  /**
   * @brief Add `elem` to the back of the queue.
   * @param elem The element to be added to the queue.
   */
  void push_back(concore2full_task* elem) noexcept {
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
  [[nodiscard]] concore2full_task* pop_front() noexcept {
    assert(!empty());
    auto old_head = head_;
    head_ = head_->next_;
    if (!head_)
      tail_ = nullptr;
    return old_head;
  }
};

} // namespace concore2full::detail
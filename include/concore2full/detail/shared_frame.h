#pragma once

#include <memory>
#include <utility>

namespace concore2full::detail {

//! Frame that is allocated on the heap.
template <typename Frame> struct shared_frame {
  using result_t = typename Frame::result_t;

  template <typename... Ts>
  explicit shared_frame(Ts&&... args)
      : frame_(std::make_shared<Frame>(std::forward<Ts>(args)...)) {}

  void spawn() { frame_->spawn(); }

  result_t await() { return frame_->await(); }

private:
  //! Wrap the frame object within a shared pointer.
  std::shared_ptr<Frame> frame_;
};

} // namespace concore2full::detail

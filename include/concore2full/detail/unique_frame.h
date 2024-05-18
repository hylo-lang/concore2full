#pragma once

#include <memory>
#include <utility>

namespace concore2full::detail {

//! Frame that is allocated on the heap, held as unique_ptr.
template <typename Frame> struct unique_frame {
  using result_t = typename Frame::result_t;

  explicit unique_frame(std::unique_ptr<Frame>&& frame) : frame_(std::move(frame)) {}

  void spawn() { frame_->spawn(); }

  result_t await() { return frame_->await(); }

private:
  //! Wrap the frame object within an unique pointer.
  std::unique_ptr<Frame> frame_;
};

} // namespace concore2full::detail

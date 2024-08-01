#pragma once

#include "concore2full/detail/raw_delete.h"

#include <memory>
#include <utility>

namespace concore2full::detail {

//! Frame that is allocated on the heap, held as unique_ptr.
template <typename Frame> struct unique_frame {
  using result_t = typename Frame::result_t;

  explicit unique_frame(raw_unique_ptr<Frame>&& frame) : frame_(std::move(frame)) {}

  void spawn() { frame_->spawn(); }

  result_t await() { return frame_->await(); }

private:
  //! Wrap the frame object within an unique pointer.
  raw_unique_ptr<Frame> frame_;
};

} // namespace concore2full::detail

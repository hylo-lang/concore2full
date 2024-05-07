#pragma once

#include <memory>
#include <utility>

namespace concore2full::detail {

//! Holder for a spawn frame, that can be either a shared_ptr or a direct object.
template <typename Frame, bool Escaping = false> struct frame_holder {
  using frame_t = Frame;

  template <typename... Ts>
  explicit frame_holder(Ts&&... args) : frame_(std::forward<Ts>(args)...) {}
  //! No copy, no move
  frame_holder(const frame_holder&) = delete;

  //! Get the inner frame object.
  frame_t& get() noexcept { return frame_; }

private:
  //! Store the frame object inplace.
  frame_t frame_;
};

template <typename Frame> struct frame_holder<Frame, true> {
  using frame_t = Frame;

  template <typename... Ts>
  explicit frame_holder(Ts&&... args)
      : frame_(std::make_shared<frame_t>(std::forward<Ts>(args)...)) {}

  //! Get the inner frame object.
  frame_t& get() noexcept { return *frame_.get(); }

private:
  //! Wrap the frame object within a shared pointer.
  std::shared_ptr<frame_t> frame_;
};

} // namespace concore2full::detail

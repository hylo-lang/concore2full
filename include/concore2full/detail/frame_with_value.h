#pragma once

#include "concore2full/c/spawn.h"
#include "concore2full/detail/spawn_frame_base.h"
#include "concore2full/detail/value_holder.h"

#include <memory>
#include <type_traits>

namespace concore2full::detail {

//! Holds a base spawn frame and the result produced by the spawn function.
template <typename FrameBase, typename Fn>
struct frame_with_value : FrameBase, value_holder<std::invoke_result_t<Fn>> {
  Fn f_;

  using value_holder_t = detail::value_holder<std::invoke_result_t<Fn>>;
  using result_t = typename value_holder_t::value_t;

  explicit frame_with_value(Fn&& f) : f_(std::forward<Fn>(f)) {}

  frame_with_value(frame_with_value&& other) : f_(std::move(other.f_)) {}

  void spawn() {
    // Also initialize the base frame.
    FrameBase::spawn(&to_execute);
  }

private:
  static void to_execute(typename FrameBase::interface_t* frame) noexcept {
    auto* d = static_cast<frame_with_value*>(FrameBase::from_interface(frame));

    if constexpr (std::is_same_v<result_t, void>) {
      std::invoke(std::forward<Fn>(d->f_));
    } else {
      static_cast<value_holder_t*>(d)->value() = std::invoke(std::forward<Fn>(d->f_));
    }
  }
};

} // namespace concore2full::detail
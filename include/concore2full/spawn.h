#pragma once

#include "concore2full/c/spawn.h"
#include "concore2full/detail/spawn_frame_base.h"
#include "concore2full/detail/value_holder.h"

#include <memory>
#include <type_traits>

namespace concore2full {

namespace detail {

//! Holds core spawn frame, the spawn function and the result of the spawn function.
template <typename Fn>
struct full_spawn_frame : spawn_frame_base, value_holder<std::invoke_result_t<Fn>> {
  Fn f_;

  using value_holder_t = detail::value_holder<std::invoke_result_t<Fn>>;
  using res_t = typename value_holder_t::value_t;

  explicit full_spawn_frame(Fn&& f) : f_(std::forward<Fn>(f)) {}

  full_spawn_frame(full_spawn_frame&& other) : f_(std::move(other.f_)) {}

  void spawn() {
    // Also initialize the base frame.
    spawn_frame_base::spawn(&to_execute);
  }

private:
  static void to_execute(concore2full_spawn_frame* frame) noexcept {
    auto* d = static_cast<detail::full_spawn_frame<Fn>*>(spawn_frame_base::from_interface(frame));

    if constexpr (std::is_same_v<res_t, void>) {
      std::invoke(std::forward<Fn>(d->f_));
    } else {
      static_cast<value_holder_t*>(d)->value() = std::invoke(std::forward<Fn>(d->f_));
    }
  }
};

//! Holder for a spawn frame, that can be either a shared_ptr or a direct object.
template <typename Fn, bool Escaping = false> struct frame_holder {
  using frame_t = full_spawn_frame<Fn>;

  explicit frame_holder(Fn&& f) : frame_(std::forward<Fn>(f)) {}
  //! No copy, no move
  frame_holder(const frame_holder&) = delete;

  frame_t& get() noexcept { return frame_; }

private:
  frame_t frame_;
};

template <typename Fn> struct frame_holder<Fn, true> {
  using frame_t = full_spawn_frame<Fn>;

  explicit frame_holder(Fn&& f) : frame_(std::make_shared<frame_t>(std::forward<Fn>(f))) {}

  frame_t& get() noexcept { return *frame_.get(); }

private:
  std::shared_ptr<frame_t> frame_;
};

} // namespace detail

//! A future object resulting from a `spawn` call.
//! The `await()` method needs to be called exactly once.
template <typename Fn, bool Escaping> class spawn_future {
public:
  ~spawn_future() = default;

  //! The type returned by the spawned computation.
  using res_t = typename detail::full_spawn_frame<Fn>::res_t;

  /**
   * @brief Await the result of the computation.
   * @return The result of the computation; throws if the operation was cancelled.
   *
   * If the main thread of execution arrives at this point after the work is done, this will simply
   * return the result of the work. If the main thread of execution arrives here before the work is
   * completed, a "thread inversion" happens, and the main thread of execution continues work on
   * the coroutine that was just spawned.
   */
  res_t await() {
    frame_.get().await();
    return frame_.get().value();
  }

private:
  //! The frame holding the state of the spawned computation.
  detail::frame_holder<Fn, Escaping> frame_;

  template <typename F> friend auto spawn(F&& f);
  template <typename F> friend auto escaping_spawn(F&& f);

  //! Private constructor. `spawn` will call this.
  //! We rely on the fact that the object will be constructed in its final destination storage.
  spawn_future(Fn&& f) : frame_(std::forward<Fn>(f)) { frame_.get().spawn(); }
};

/**
 * @brief Spawn work with the default scheduler.
 * @tparam Fn The type of the function to execute.
 * @param f The function representing the work that needs to be executed asynchronously.
 * @return A `spawn_future` object; this object cannot be copied or moved
 *
 * This will use the default scheduler to spawn new work concurrently.
 *
 * The returned state object needs to stay alive for the entire duration of the computation.
 */
template <typename Fn> inline auto spawn(Fn&& f) {
  return spawn_future<Fn, false>{std::forward<Fn>(f)};
}

//! Same as `spawn`, but the returned future can be copied and moved.
//! The caller is responsible for calling `await` exactly once on the returned object.
template <typename Fn> inline auto escaping_spawn(Fn&& f) {
  return spawn_future<Fn, true>{std::forward<Fn>(f)};
}

} // namespace concore2full
#pragma once

#include "concore2full/c/spawn.h"
#include "concore2full/detail/frame_holder.h"
#include "concore2full/detail/frame_with_value.h"
#include "concore2full/detail/spawn_frame_base.h"
#include "concore2full/detail/value_holder.h"

#include <utility>

namespace concore2full {

//! A future object resulting from a `spawn` call.
//! The `await()` method needs to be called exactly once.
template <typename Fn, bool Escaping> class spawn_future {
public:
  ~spawn_future() = default;

  //! The type returned by the spawned computation.
  using res_t = typename detail::frame_with_value<detail::spawn_frame_base, Fn>::res_t;

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
  detail::frame_holder<detail::frame_with_value<detail::spawn_frame_base, Fn>, Escaping> frame_;

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
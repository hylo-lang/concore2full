#pragma once

#include "concore2full/c/spawn.h"
#include "concore2full/detail/frame_with_value.h"
#include "concore2full/detail/shared_frame.h"
#include "concore2full/detail/spawn_frame_base.h"
#include "concore2full/future.h"

#include <utility>

namespace concore2full {

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
  using frame_holder_t = detail::frame_with_value<detail::spawn_frame_base, Fn>;
  return future<frame_holder_t>{detail::start_spawn_t{}, std::forward<Fn>(f)};
}

//! Same as `spawn`, but the returned future can be copied and moved.
//! The caller is responsible for calling `await` exactly once on the returned object.
template <typename Fn> inline auto escaping_spawn(Fn&& f) {
  using frame_holder_t =
      detail::shared_frame<detail::frame_with_value<detail::spawn_frame_base, Fn>>;
  return future<frame_holder_t>{detail::start_spawn_t{}, std::forward<Fn>(f)};
}

} // namespace concore2full
#pragma once

#include "concore2full/c/spawn.h"
#include "concore2full/detail/bulk_spawn_frame_full.h"
#include "concore2full/detail/copyable_spawn_frame_base.h"
#include "concore2full/detail/frame_with_value.h"
#include "concore2full/detail/shared_frame.h"
#include "concore2full/detail/spawn_frame_base.h"
#include "concore2full/detail/unique_frame.h"
#include "concore2full/future.h"

#include <concepts>
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
template <std::invocable Fn> inline auto spawn(Fn&& f) {
  using frame_holder_t = detail::frame_with_value<detail::spawn_frame_base, Fn>;
  return future<frame_holder_t>{detail::start_spawn_t{}, std::forward<Fn>(f)};
}

//! Same as `spawn`, but the returned future can be copied and moved.
//! The caller is responsible for calling `await` exactly once on the returned object.
template <std::invocable Fn> inline auto escaping_spawn(Fn&& f) {
  using frame_holder_t =
      detail::shared_frame<detail::frame_with_value<detail::spawn_frame_base, Fn>>;
  return future<frame_holder_t>{detail::start_spawn_t{}, std::forward<Fn>(f)};
}

//! Same as `spawn`, but the returned future can be copied and moved.
//! The caller is responsible for calling `await` exactly once on each copy of the returned object.
template <std::invocable Fn> inline auto copyable_spawn(Fn&& f) {
  using frame_holder_t =
      detail::shared_frame<detail::frame_with_value<detail::copyable_spawn_frame_base, Fn>>;
  return future<frame_holder_t>{detail::start_spawn_t{}, std::forward<Fn>(f)};
}

/**
 * @brief Bulk spawn work with the default scheduler.
 * @tparam Fn The type of the function to execute.
 * @param count The number of workers to spawn for handling the bulk work.
 * @param f The function representing the work that needs to be executed asynchronously.
 * @return A `bulk_spawn_future` object; this object cannot be copied or moved
 *
 * This will use the default scheduler to spawn new work concurrently.
 *
 * The returned state object needs to stay alive for the entire duration of the computation.
 */
template <typename Fn> inline auto bulk_spawn(int count, Fn&& f) {
  assert(count > 0);
  using frame_holder_t = detail::unique_frame<detail::bulk_spawn_frame_full<Fn>>;
  auto uptr = detail::bulk_spawn_frame_full<Fn>::allocate(count, std::forward<Fn>(f));
  return future<frame_holder_t>{detail::start_spawn_t{}, std::move(uptr)};
}

} // namespace concore2full
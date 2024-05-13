#pragma once

#include "concore2full/detail/bulk_spawn_frame_full.h"
#include "concore2full/detail/unique_frame.h"
#include "concore2full/future.h"

#include <cassert>

namespace concore2full {

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
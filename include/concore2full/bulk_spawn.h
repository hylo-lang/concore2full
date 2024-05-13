#pragma once

#include "concore2full/c/bulk_spawn.h"
#include "concore2full/detail/bulk_spawn_frame_full.h"

#include <cassert>
#include <memory>

namespace concore2full {

//! A future object resulting from a `bulk_spawn` call.
//!
//! The `await()` method needs to be called exactly once.
template <typename Fn> class bulk_spawn_future {
public:
  ~bulk_spawn_future() = default;

  bulk_spawn_future(bulk_spawn_future&&) = default;
  bulk_spawn_future(const bulk_spawn_future&) = delete;

  //! Await the result of the computation.
  //!
  //! May return on a different thread (i.e., the one that last finishes the work)
  void await() { frame_->base_frame_.await(); }

private:
  //! The frame holding the state of the spawned computation.
  std::unique_ptr<detail::bulk_spawn_frame_full<Fn>> frame_;

  template <typename F> friend auto bulk_spawn(int count, F&& f);

  //! Private constructor. `bulk_spawn` will call this.
  //! We rely on the fact that the object will be constructed in its final destination storage.
  bulk_spawn_future(int count, Fn&& f)
      : frame_(detail::bulk_spawn_frame_full<Fn>::allocate(count, std::forward<Fn>(f))) {
    frame_->base_frame_.spawn(count, &detail::bulk_spawn_frame_full<Fn>::to_execute);
  }
};

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
  return bulk_spawn_future<Fn>{count, std::forward<Fn>(f)};
}

} // namespace concore2full
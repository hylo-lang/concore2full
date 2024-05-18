#pragma once

#include "concore2full/c/spawn.h"

#include <utility>

namespace concore2full {

namespace detail {
//! Tag type to indicate that a spawn operation is starting.
struct start_spawn_t {};
} // namespace detail

//! An asynchronous computation created from a `spawn`-like call.
//! The `await()` method needs to be called exactly once.
template <typename FrameHolder> class future {
public:
  ~future() = default;

  //! Construct the future and spawns the required computation.
  //! We rely on the fact that the object will be constructed in its final destination storage.
  template <typename... Ts>
  future(detail::start_spawn_t, Ts&&... ts) : frame_(std::forward<Ts>(ts)...) {
    frame_.spawn();
  }

  //! The type of the value that can be awaited on..
  using result_t = typename FrameHolder::result_t;

  /**
   * @brief Await the result of the computation.
   * @return The result of the computation; throws if the operation was cancelled.
   *
   * Postconditions:
   *  - the spawned computation is completed.
   *  - the result of the computation is returned.
   *
   * Note:
   *  - the exit thread may be different from the thread that called this method.
   */
  result_t await() { return frame_.await(); }

private:
  //! The frame holding the state of the spawned computation.
  FrameHolder frame_;
};

} // namespace concore2full
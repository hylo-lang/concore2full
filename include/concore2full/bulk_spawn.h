#pragma once

#include "concore2full/c/bulk_spawn.h"

#include <cassert>
#include <functional>
#include <memory>

namespace concore2full {

namespace detail {

//! Represents the frame for a bulk spawn operation.
template <typename Fn> struct bulk_spawn_frame {
  //! The use function to execute multiple times in parallel.
  Fn f_;
  //! The base frame for the bulk spawn operation, containing implementation details.
  concore2full_bulk_spawn_frame base_frame_;
  // Note: we occupy more space after `base_frame_` to store the tasks and the thread suspension.

  //! Allocates a frame for bulk spawning `count` tasks that call `f`.
  static std::unique_ptr<bulk_spawn_frame> allocate(int count, Fn&& f) {
    size_t size_base_frame = concore2full_frame_size(count);
    size_t total_size =
        sizeof(bulk_spawn_frame) - sizeof(struct concore2full_bulk_spawn_frame) + size_base_frame;
    void* p = operator new(total_size);
    try {
      return std::unique_ptr<bulk_spawn_frame>{new (p) bulk_spawn_frame(std::forward<Fn>(f))};
    } catch (...) {
      operator delete(p);
      throw;
    }
  }

  bulk_spawn_frame(bulk_spawn_frame&&) = delete;
  bulk_spawn_frame(const bulk_spawn_frame&) = delete;

  //! The function called by the bulk spawn API to execute the work.
  static void to_execute(concore2full_bulk_spawn_frame* frame, uint64_t index) noexcept {
    char* p = reinterpret_cast<char*>(frame);
    bulk_spawn_frame* self =
        reinterpret_cast<bulk_spawn_frame*>(p - offsetOf(&bulk_spawn_frame::base_frame_));

    std::invoke(std::forward<Fn>(self->f_), index);
  }

private:
  explicit bulk_spawn_frame(Fn&& f) : f_(std::forward<Fn>(f)) {}
};

template <typename T, typename U> constexpr size_t offsetOf(U T::*member) {
  return (char*)&((T*)nullptr->*member) - (char*)nullptr;
}

} // namespace detail

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
  void await() { concore2full_bulk_await(&frame_->base_frame_); }

private:
  //! The frame holding the state of the spawned computation.
  std::unique_ptr<detail::bulk_spawn_frame<Fn>> frame_;

  template <typename F> friend auto bulk_spawn(int count, F&& f);

  //! Private constructor. `bulk_spawn` will call this.
  //! We rely on the fact that the object will be constructed in its final destination storage.
  bulk_spawn_future(int count, Fn&& f)
      : frame_(detail::bulk_spawn_frame<Fn>::allocate(count, std::forward<Fn>(f))) {
    concore2full_bulk_spawn(&frame_->base_frame_, count, &detail::bulk_spawn_frame<Fn>::to_execute);
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
#pragma once

#include "concore2full/detail/bulk_spawn_frame_base.h"

#include <functional>
#include <memory>

namespace concore2full::detail {

//! Represents the frame for a bulk spawn operation.
template <typename Fn> struct bulk_spawn_frame_full {
  //! The use function to execute multiple times in parallel.
  Fn f_;
  //! The base frame for the bulk spawn operation, containing implementation details.
  bulk_spawn_frame_base base_frame_;
  // Note: we occupy more space after `base_frame_` to store the tasks and the thread suspension.

  //! Allocates a frame for bulk spawning `count` tasks that call `f`.
  static std::unique_ptr<bulk_spawn_frame_full> allocate(int count, Fn&& f) {
    size_t size_base_frame = bulk_spawn_frame_base::frame_size(count);
    size_t total_size =
        sizeof(bulk_spawn_frame_full) - sizeof(bulk_spawn_frame_base) + size_base_frame;
    void* p = operator new(total_size);
    try {
      return std::unique_ptr<bulk_spawn_frame_full>{new (p)
                                                        bulk_spawn_frame_full(std::forward<Fn>(f))};
    } catch (...) {
      operator delete(p);
      throw;
    }
  }

  bulk_spawn_frame_full(bulk_spawn_frame_full&&) = delete;
  bulk_spawn_frame_full(const bulk_spawn_frame_full&) = delete;

  //! The function called by the bulk spawn API to execute the work.
  static void to_execute(concore2full_bulk_spawn_frame* frame, uint64_t index) noexcept {
    char* p = reinterpret_cast<char*>(frame);
    bulk_spawn_frame_full* self =
        reinterpret_cast<bulk_spawn_frame_full*>(p - offsetOf(&bulk_spawn_frame_full::base_frame_));

    std::invoke(std::forward<Fn>(self->f_), index);
  }

private:
  explicit bulk_spawn_frame_full(Fn&& f) : f_(std::forward<Fn>(f)) {}
};

template <typename T, typename U> constexpr size_t offsetOf(U T::*member) {
  return (char*)&((T*)nullptr->*member) - (char*)nullptr;
}

} // namespace concore2full::detail

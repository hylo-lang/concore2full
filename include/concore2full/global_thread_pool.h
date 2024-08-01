#pragma once

#include "concore2full/thread_pool.h"
#include "concore2full/thread_snapshot.h"

namespace concore2full {

namespace detail {
/// Wrapper around the global thread pool, to be used in the `spawn()` function.
/// Ensures that shutdown of the global thread pool is done on the same OS thread that started it.
struct global_thread_pool_wrapper {
  thread_pool wrapped_;
  thread_snapshot snapshot_;

  /// Constructs the wrapped thread pool, remembering the OS thread.
  global_thread_pool_wrapper() = default;
  /// Reverts the thread snapshot and stops the wrapped thread pool.
  ~global_thread_pool_wrapper() {
    snapshot_.revert();
    wrapped_.join();
  }
};
} // namespace detail

inline thread_pool& global_thread_pool() {
  static detail::global_thread_pool_wrapper instance;
  return instance.wrapped_;
}

} // namespace concore2full
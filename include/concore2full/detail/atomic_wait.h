#pragma once

#include "concore2full/c/atomic_wrapper.h"

#include <atomic>
#include <thread>

namespace concore2full::detail {

//! Wait until the given function returns true.
template <typename F> inline void wait_with_backoff(F&& f) {
  constexpr int polling_count = 64;
  uint32_t i = 0;
  while (true) {
    if (f())
      return;
    // Do some polling.
    if (i++ < polling_count)
      continue;

    // Yield the control for this OS thread.
    std::this_thread::yield();
    i = 0;
  }
}

//! Wait until the given function, applied to the value read from `a`, returns true.
template <typename T, typename F> inline void atomic_wait(CONCORE2FULL_ATOMIC(T) & a, F&& f) {
  wait_with_backoff([&a, f = std::forward<F>(f)]() {
    return f(atomic_load_explicit(&a, std::memory_order_acquire));
  });
}

}
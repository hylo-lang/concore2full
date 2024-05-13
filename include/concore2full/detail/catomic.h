#pragma once

#include <atomic>

namespace concore2full::detail {

//! Atomic type that is also copyable.
template <typename T> struct catomic : public std::atomic<T> {
  using base_t = std::atomic<T>;

  catomic() : base_t(0) {}
  catomic(T value) : base_t(value) {}

  catomic(const catomic& other) : base_t(other.load(std::memory_order_relaxed)) {}
  catomic& operator=(const catomic& other) {
    base_t::store(other.load(std::memory_order_relaxed), std::memory_order_relaxed);
    return *this;
  };

  catomic(catomic&&) = default;
  catomic& operator=(catomic&&) = default;
};

} // namespace concore2full::detail

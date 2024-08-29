#pragma once

#include "profiling.h"
#include <concore2full/detail/catomic.h>

namespace concore2full::profiling {

//! Wraper over `catomic` that, when using profiling, will instrument all the operations with the
//! atomic.
template <std::integral T> struct atomic : detail::catomic<T> {
  using base_t = detail::catomic<T>;

  atomic() : base_t() {}
  explicit atomic(T value) : base_t(value) {}

  atomic(const atomic&) = default;
  atomic& operator=(const atomic&) = default;
  atomic(atomic&&) = default;
  atomic& operator=(atomic&&) = default;

  //! Gives a name to this atomic variable, for profiling purposes, creating a counter track for it.
  void set_name(const char* name) {
#if USE_PROFILING_LITE
    char buf[64];
    snprintf(buf, sizeof(buf), "%s_%p", name, this);
    auto tid = reinterpret_cast<uint64_t>(this);
    low_level::define_counter_track(tid, buf);
    profiling::low_level::emit_counter_value(
        tid, static_cast<int64_t>(base_t::load(std::memory_order_relaxed)));
#else
    (void)name;
#endif
  }

  T operator=(T v) volatile noexcept {
    store(v);
    return v;
  }

  T operator=(T v) noexcept {
    store(v);
    return v;
  }

  void store(T v, std::memory_order order = std::memory_order_seq_cst) noexcept {
    auto t0 = now();
    base_t::store(v, order);
    do_trace(t0, "store", v);
  }
  T load(std::memory_order order = std::memory_order_seq_cst) const volatile noexcept {
    auto t0 = now();
    auto v = base_t::load(order);
    do_trace(t0, "load", v);
    return v;
  }
  T load(std::memory_order order = std::memory_order_seq_cst) const noexcept {
    auto t0 = now();
    auto v = base_t::load(order);
    do_trace(t0, "load", v);
    return v;
  }

  operator T() const volatile noexcept { return load(); }
  operator T() const noexcept { return load(); }

  T exchange(T v, std::memory_order order = std::memory_order_seq_cst) volatile noexcept {
    auto t0 = now();
    auto r = base_t::exchange(v, order);
    do_trace(t0, "exchange", v);
    return r;
  }
  T exchange(T v, std::memory_order order = std::memory_order_seq_cst) noexcept {
    auto t0 = now();
    auto r = base_t::exchange(v, order);
    do_trace(t0, "exchange", v);
    return r;
  }
  bool compare_exchange_weak(T& expected, T v, std::memory_order success_order,
                             std::memory_order fail_order) volatile noexcept {
    auto t0 = now();
    bool r = base_t::compare_exchange_weak(expected, v, success_order, fail_order);
    if (r)
      do_trace(t0, "compare_exchange_weak (success)", v);
    else
      do_trace(t0, "compare_exchange_weak (fail)", expected);
    return r;
  }
  bool compare_exchange_weak(T& expected, T v, std::memory_order success_order,
                             std::memory_order fail_order) noexcept {
    auto t0 = now();
    bool r = base_t::compare_exchange_weak(expected, v, success_order, fail_order);
    if (r)
      do_trace(t0, "compare_exchange_weak (success)", v);
    else
      do_trace(t0, "compare_exchange_weak (fail)", expected);
    return r;
  }
  bool compare_exchange_strong(T& expected, T v, std::memory_order success_order,
                               std::memory_order fail_order) volatile noexcept {
    auto t0 = now();
    bool r = base_t::compare_exchange_strong(expected, v, success_order, fail_order);
    if (r)
      do_trace(t0, "compare_exchange_strong (success)", v);
    else
      do_trace(t0, "compare_exchange_strong (fail)", expected);
    return r;
  }
  bool compare_exchange_strong(T& expected, T v, std::memory_order success_order,
                               std::memory_order fail_order) noexcept {
    auto t0 = now();
    bool r = base_t::compare_exchange_strong(expected, v, success_order, fail_order);
    if (r)
      do_trace(t0, "compare_exchange_strong (success)", v);
    else
      do_trace(t0, "compare_exchange_strong (fail)", expected);
    return r;
  }
  bool
  compare_exchange_weak(T& expected, T v,
                        std::memory_order order = std::memory_order_seq_cst) volatile noexcept {
    auto t0 = now();
    bool r = base_t::compare_exchange_weak(expected, v, order);
    if (r)
      do_trace(t0, "compare_exchange_weak (success)", v);
    else
      do_trace(t0, "compare_exchange_weak (fail)", expected);
    return r;
  }
  bool compare_exchange_weak(T& expected, T v,
                             std::memory_order order = std::memory_order_seq_cst) noexcept {
    auto t0 = now();
    bool r = base_t::compare_exchange_weak(expected, v, order);
    if (r)
      do_trace(t0, "compare_exchange_weak (success)", v);
    else
      do_trace(t0, "compare_exchange_weak (fail)", expected);
    return r;
  }
  bool
  compare_exchange_strong(T& expected, T v,
                          std::memory_order order = std::memory_order_seq_cst) volatile noexcept {
    auto t0 = now();
    bool r = base_t::compare_exchange_strong(expected, v, order);
    if (r)
      do_trace(t0, "compare_exchange_strong (success)", v);
    else
      do_trace(t0, "compare_exchange_strong (fail)", expected);
    return r;
  }
  bool compare_exchange_strong(T& expected, T v,
                               std::memory_order order = std::memory_order_seq_cst) noexcept {
    auto t0 = now();
    bool r = base_t::compare_exchange_strong(expected, v, order);
    if (r)
      do_trace(t0, "compare_exchange_strong (success)", v);
    else
      do_trace(t0, "compare_exchange_strong (fail)", expected);
    return r;
  }

  void wait(T v, std::memory_order order = std::memory_order_seq_cst) const volatile noexcept {
    auto t0 = now();
    base_t::wait(v, order);
    do_trace(t0, "wait");
  }
  void wait(T v, std::memory_order order = std::memory_order_seq_cst) const noexcept {
    auto t0 = now();
    base_t::wait(v, order);
    do_trace(t0, "wait");
  }
  void notify_one() volatile noexcept {
    auto t0 = now();
    base_t::notify_one();
    do_trace(t0, "notify_one");
  }
  void notify_one() noexcept {
    auto t0 = now();
    base_t::notify_one();
    do_trace(t0, "notify_one");
  }
  void notify_all() volatile noexcept {
    auto t0 = now();
    base_t::notify_all();
    do_trace(t0, "notify_all");
  }
  void notify_all() noexcept {
    auto t0 = now();
    base_t::notify_all();
    do_trace(t0, "notify_all");
  }

  T fetch_add(T v, std::memory_order order = std::memory_order_seq_cst) volatile noexcept {
    auto t0 = now();
    T r = base_t::fetch_add(v, order);
    do_trace(t0, "fetch_add", r + 1);
    return r;
  }
  T fetch_add(T v, std::memory_order order = std::memory_order_seq_cst) noexcept {
    auto t0 = now();
    T r = base_t::fetch_add(v, order);
    do_trace(t0, "fetch_add", r + 1);
    return r;
  }
  T fetch_sub(T v, std::memory_order order = std::memory_order_seq_cst) volatile noexcept {
    auto t0 = now();
    T r = base_t::fetch_sub(v, order);
    do_trace(t0, "fetch_sub", r - 1);
    return r;
  }
  T fetch_sub(T v, std::memory_order order = std::memory_order_seq_cst) noexcept {
    auto t0 = now();
    T r = base_t::fetch_sub(v, order);
    do_trace(t0, "fetch_sub", r - 1);
    return r;
  }
  T fetch_and(T v, std::memory_order order = std::memory_order_seq_cst) volatile noexcept {
    auto t0 = now();
    T r = base_t::fetch_and(v, order);
    do_trace(t0, "fetch_and");
    return r;
  }
  T fetch_and(T v, std::memory_order order = std::memory_order_seq_cst) noexcept {
    auto t0 = now();
    T r = base_t::fetch_and(v, order);
    do_trace(t0, "fetch_and");
    return r;
  }
  T fetch_or(T v, std::memory_order order = std::memory_order_seq_cst) volatile noexcept {
    auto t0 = now();
    T r = base_t::fetch_or(v, order);
    do_trace(t0, "fetch_or");
    return r;
  }
  T fetch_or(T v, std::memory_order order = std::memory_order_seq_cst) noexcept {
    auto t0 = now();
    T r = base_t::fetch_or(v, order);
    do_trace(t0, "fetch_or");
    return r;
  }
  T fetch_xor(T v, std::memory_order order = std::memory_order_seq_cst) volatile noexcept {
    auto t0 = now();
    T r = base_t::fetch_xor(v, order);
    do_trace(t0, "fetch_xor");
    return r;
  }
  T fetch_xor(T v, std::memory_order order = std::memory_order_seq_cst) noexcept {
    auto t0 = now();
    T r = base_t::fetch_xor(v, order);
    do_trace(t0, "fetch_xor");
    return r;
  }

  T operator++(int) volatile noexcept { return fetch_add(T(1)); }
  T operator++(int) noexcept { return fetch_add(T(1)); }
  T operator--(int) volatile noexcept { return fetch_sub(T(1)); }
  T operator--(int) noexcept { return fetch_sub(T(1)); }
  T operator++() volatile noexcept { return fetch_add(T(1)) + T(1); }
  T operator++() noexcept { return fetch_add(T(1)) + T(1); }
  T operator--() volatile noexcept { return fetch_sub(T(1)) - T(1); }
  T operator--() noexcept { return fetch_sub(T(1)) - T(1); }
  T operator+=(T v) volatile noexcept { return fetch_add(v) + v; }
  T operator+=(T v) noexcept { return fetch_add(v) + v; }
  T operator-=(T v) volatile noexcept { return fetch_sub(v) - v; }
  T operator-=(T v) noexcept { return fetch_sub(v) - v; }
  T operator&=(T v) volatile noexcept { return fetch_and(v) & v; }
  T operator&=(T v) noexcept { return fetch_and(v) & v; }
  T operator|=(T v) volatile noexcept { return fetch_or(v) | v; }
  T operator|=(T v) noexcept { return fetch_or(v) | v; }
  T operator^=(T v) volatile noexcept { return fetch_xor(v) ^ v; }
  T operator^=(T v) noexcept { return fetch_xor(v) ^ v; }

private:
#if USE_PROFILING_LITE
  using timestamp_t = profiling_lite::timestamp_t;
  static timestamp_t now() { return profiling_lite::now(); }
#else
  using timestamp_t = uint64_t;
  static timestamp_t now() { return 0; }
#endif

  void do_trace(timestamp_t t0, std::string_view name, T value) const {
#if USE_PROFILING_LITE
    auto t1 = profiling_lite::now();
    void* stack_ptr = &t1;
    profiling_lite::emit_zone_start(stack_ptr, profiling_lite::get_current_thread(), t0,
                                    CURRENT_LOCATION());
    profiling_lite::emit_zone_dynamic_name(stack_ptr, name.data());
    auto counter_ptr_value = reinterpret_cast<uint64_t>(this);
    profiling_lite::emit_zone_param(stack_ptr, "counter_ptr,x", counter_ptr_value);
    profiling_lite::emit_zone_param(stack_ptr, "value", static_cast<int64_t>(value));
    profiling_lite::emit_zone_flow(stack_ptr, counter_ptr_value);
    profiling_lite::emit_counter_value(counter_ptr_value, t1, static_cast<int64_t>(value));
    profiling_lite::emit_zone_end(stack_ptr, t1);
#else
    (void)t0;
    (void)name;
    (void)value;
#endif
  }
  void do_trace(timestamp_t t0, std::string_view name) const {
#if USE_PROFILING_LITE
    do_trace(t0, name, base_t::load(std::memory_order_relaxed));
#else
    (void)t0;
    (void)name;
#endif
  }
};

} // namespace concore2full::profiling
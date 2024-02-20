#pragma once

#include <string_view>
#include <thread>

// #define USE_PROFILING_LITE 1

#if USE_PROFILING_LITE

#include "profiling-lite.hpp"

#define CURRENT_LOCATION() PROFILING_LITE_CURRENT_LOCATION()
#define CURRENT_LOCATION_N(name) PROFILING_LITE_CURRENT_LOCATION_N(name)

namespace concore2full::profiling {

struct zone : private profiling_lite::zone {
  using base_t = profiling_lite::zone;

  explicit zone(profiling_lite::location* loc) : base_t(loc) {}
  ~zone() = default;

  void set_dyn_name(std::string_view name) { base_t::set_dyn_name(name); }
  void set_param(const char* static_name, bool value) { base_t::set_param(static_name, value); }
  void set_param(const char* static_name, uint64_t value) { base_t::set_param(static_name, value); }
  void set_param(const char* static_name, int64_t value) { base_t::set_param(static_name, value); }
  void set_param(const char* static_name, std::string_view name) {
    base_t::set_param(static_name, name);
  }
  void add_flow(uint64_t flow_id) { base_t::add_flow(flow_id); }
  void add_flow_terminate(uint64_t flow_id) { base_t::add_flow_terminate(flow_id); }
  void set_category(const char* static_name) { base_t::set_category(static_name); }
};

struct zone_instant : private profiling_lite::zone_instant {
  using base_t = profiling_lite::zone_instant;

  explicit zone_instant(profiling_lite::location* loc) : base_t(loc) {}
  ~zone_instant() = default;

  void set_dyn_name(std::string_view name) { base_t::set_dyn_name(name); }
  void set_param(const char* static_name, bool value) { base_t::set_param(static_name, value); }
  void set_param(const char* static_name, uint64_t value) { base_t::set_param(static_name, value); }
  void set_param(const char* static_name, int64_t value) { base_t::set_param(static_name, value); }
  void set_param(const char* static_name, std::string_view name) {
    base_t::set_param(static_name, name);
  }
  void add_flow(uint64_t flow_id) { base_t::add_flow(flow_id); }
  void add_flow_terminate(uint64_t flow_id) { base_t::add_flow_terminate(flow_id); }
  void set_category(const char* static_name) { base_t::set_category(static_name); }
};

inline void define_stack(const void* begin, const void* end, const char* name) {
  profiling_lite::define_stack(begin, end, name);
}
inline void emit_thread_name_and_stack(const char* name) {
  uint8_t dummy = 0;
  profiling_lite::define_stack(nullptr, &dummy + 0x10, name);
  profiling_lite::set_thread_name(profiling_lite::get_current_thread(), name);
}

} // namespace concore2full::profiling

#else

#define CURRENT_LOCATION() 0
#define CURRENT_LOCATION_N(name) 0

namespace concore2full::profiling {

struct zone {
  explicit zone(int dummy) {}
  ~zone() = default;

  void set_dyn_name(std::string_view name) {}
  void set_param(const char* static_name, bool value) {}
  void set_param(const char* static_name, uint64_t value) {}
  void set_param(const char* static_name, int64_t value) {}
  void set_param(const char* static_name, std::string_view name) {}
  void add_flow(uint64_t flow_id) {}
  void add_flow_terminate(uint64_t flow_id) {}
  void set_category(const char* static_name) {}
};

struct zone_instant {
  explicit zone_instant(int dummy) {}
  ~zone_instant() = default;

  void set_dyn_name(std::string_view name) {}
  void set_param(const char* static_name, bool value) {}
  void set_param(const char* static_name, uint64_t value) {}
  void set_param(const char* static_name, int64_t value) {}
  void set_param(const char* static_name, std::string_view name) {}
  void add_flow(uint64_t flow_id) {}
  void add_flow_terminate(uint64_t flow_id) {}
  void set_category(const char* static_name) {}
};

inline void define_stack(const void* begin, const void* end, const char* name) {}
inline void emit_thread_name_and_stack(const char* name) {}

} // namespace concore2full::profiling

#endif

namespace concore2full::profiling {

template <typename Duration> inline void sleep_for(Duration d) {
  zone zone{CURRENT_LOCATION()};
  zone.set_category("sleep");
  std::this_thread::sleep_for(d);
}

} // namespace concore2full::profiling

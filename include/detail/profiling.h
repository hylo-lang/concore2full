#pragma once

#include <thread>

#if USE_TRACY

#include <tracy_interface.hpp>

#define CURRENT_LOCATION()                                                                         \
  [](const char* f) -> tracy_interface::location {                                                 \
    static tracy_interface::location l{nullptr, f, __FILE__, __LINE__, 0};                         \
    return l;                                                                                      \
  }(__FUNCTION__)
#define CURRENT_LOCATION_N(name)                                                                   \
  [](const char* f) -> tracy_interface::location {                                                 \
    static tracy_interface::location l{name, f, __FILE__, __LINE__, 0};                            \
    return l;                                                                                      \
  }(__FUNCTION__)
#define CURRENT_LOCATION_C(color)                                                                  \
  [](const char* f) -> tracy_interface::location {                                                 \
    static tracy_interface::location l{nullptr, f, __FILE__, __LINE__,                             \
                                       static_cast<uint32_t>(color)};                              \
    return l;                                                                                      \
  }(__FUNCTION__)
#define CURRENT_LOCATION_NC(name, color)                                                           \
  [](const char* f) -> tracy_interface::location {                                                 \
    static tracy_interface::location l{name, f, __FILE__, __LINE__, static_cast<uint32_t>(color)}; \
    return l;                                                                                      \
  }(__FUNCTION__)

namespace profiling {

struct zone_stack_snapshot;
struct duplicate_zones_stack;

struct zone {
  explicit zone(const tracy_interface::location& loc) : location_(&loc), parent_(thread_top_zone_) {
    thread_top_zone_ = this;
    tracy_interface::emit_zone_begin(&loc);
  }

  ~zone() {
    tracy_interface::emit_zone_end();
    thread_top_zone_ = parent_;
  }

  void set_dyn_name(std::string_view name) { tracy_interface::set_dyn_name(name); }
  void set_text(std::string_view text) { tracy_interface::set_text(text); }
  void set_color(uint32_t color) { tracy_interface::set_color(color); }
  void set_value(uint64_t value) { tracy_interface::set_value(value); }

private:
  const tracy_interface::location* location_;
  zone* parent_;
  static thread_local zone* thread_top_zone_;

  friend zone_stack_snapshot;
  friend duplicate_zones_stack;
};

inline thread_local zone* zone::thread_top_zone_{nullptr};

struct zone_stack_snapshot {
  zone_stack_snapshot() : top_zone_{zone::thread_top_zone_} {}

private:
  zone* top_zone_;
  friend struct duplicate_zones_stack;
};

struct duplicate_zones_stack {
  explicit duplicate_zones_stack(zone_stack_snapshot snapshot) : top_zone_(snapshot.top_zone_) {
    zone* top_zone = snapshot.top_zone_;
    assert(zone::thread_top_zone_ == nullptr);
    zones_count_ = emit_zones_rec(top_zone, top_zone);
    zone::thread_top_zone_ = top_zone;
  }

  ~duplicate_zones_stack() {
    // Remove the copied zones
    assert(zone::thread_top_zone_ == top_zone_);
    for (zone* z = top_zone_; z && zones_count_-- > 0; z = z->parent_) {
      tracy_interface::emit_zone_end();
    }
    zone::thread_top_zone_ = nullptr;
  }

  duplicate_zones_stack(duplicate_zones_stack&& other) = delete;
  duplicate_zones_stack& operator=(duplicate_zones_stack&& other) = delete;

  duplicate_zones_stack(const duplicate_zones_stack& other) = delete;
  duplicate_zones_stack& operator=(const duplicate_zones_stack& other) = delete;

private:
  //! The zone that was on top when we started the split.
  zone* top_zone_;
  //! The number of zones we copied;
  int zones_count_{0};

  static int emit_zones_rec(zone* z, zone* base);
};

inline void set_cur_thread_name(const char* static_name) {
  tracy_interface::set_cur_thread_name(static_name);
}

} // namespace profiling

#else

#define CURRENT_LOCATION() 0
#define CURRENT_LOCATION_N(name) 0
#define CURRENT_LOCATION_C(color) 0
#define CURRENT_LOCATION_NC(name, color) 0

namespace profiling {

struct zone {
  explicit zone(int dummy) {}

  ~zone() = default;

  void set_dyn_name(std::string_view name) {}
  void set_text(std::string_view text) {}
  void set_color(uint32_t color) {}
  void set_value(uint64_t value) {}
};

struct zone_stack_snapshot {
  zone_stack_snapshot() {}
};

struct duplicate_zones_stack {
  explicit duplicate_zones_stack(zone_stack_snapshot) {}

  ~duplicate_zones_stack() {}

  duplicate_zones_stack(duplicate_zones_stack&& other) = delete;
  duplicate_zones_stack& operator=(duplicate_zones_stack&& other) = delete;

  duplicate_zones_stack(const duplicate_zones_stack& other) = delete;
  duplicate_zones_stack& operator=(const duplicate_zones_stack& other) = delete;
};

inline void set_cur_thread_name(const char* static_name) {}

} // namespace profiling

#endif

namespace profiling {

enum class color {
  automatic = 0,
  gray = 0x808080,
  green = 0x008000,
};

template <typename Duration> inline void sleep_for(Duration d) {
  zone zone{CURRENT_LOCATION_C(color::gray)};
  std::this_thread::sleep_for(d);
}

} // namespace profiling

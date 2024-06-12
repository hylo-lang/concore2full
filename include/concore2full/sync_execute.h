#pragma once

#include <concore2full/profiling.h>
#include <concore2full/thread_snapshot.h>

#include <concepts>
#include <functional>

namespace concore2full {

template <std::invocable Fn> auto sync_execute(Fn&& f) {
  // Helper class to get back to the original thread on destruction.
  struct scoped_thread_pinpoint {
    scoped_thread_pinpoint() = default;
    ~scoped_thread_pinpoint() {
      concore2full::profiling::zone zone{CURRENT_LOCATION()};
      zone.set_category("blocking");
      // Ensure we get back on the original thread we had on constructor.
      t_.revert();
    }
    thread_snapshot t_;
  };
  scoped_thread_pinpoint thread_pinpoint;

  // Invoke the given function.
  // After invoking the function we may continue from a different thread; however, out thread
  // pinpoint will revert to the original thread.
  return std::invoke(std::forward<Fn>(f));
}

} // namespace concore2full

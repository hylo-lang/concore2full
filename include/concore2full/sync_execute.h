#pragma once

#include <concore2full/thread_control_helper.h>

#include <concepts>

namespace concore2full {

template <std::invocable Fn> auto sync_execute(Fn&& f) {
  // Mark this thread as one that may want to perform a reverse thread inversion.
  thread_control_helper t;

  // Invoke the given function.
  // We may continue from a different thread
  std::invoke(std::forward<Fn>(f));

  t.ensure_starting_thread();

  // TODO: return value
  return;
}

} // namespace concore2full
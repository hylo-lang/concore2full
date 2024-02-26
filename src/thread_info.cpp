#include "thread_info.h"

#include <cassert>
#include <mutex>

namespace concore2full::detail {

//! The data associated with each thread.
thread_local thread_info tls_thread_info;

//! Global mutex to track dependencies between threads when requesting thread switch.
static std::mutex g_thread_dependency_bottleneck;

bool thread_switch_control::request_switch_to(thread_info* self, thread_info* t) {
  assert(self != t);
  std::lock_guard<std::mutex> lock{g_thread_dependency_bottleneck};
  if (!t->switch_control_.is_currently_switching_) {
    // Mark the start of the switch.
    is_currently_switching_ = true;
    waiting_on_thread_ = t;
    return true;
  } else {
    return false;
  }
}
void thread_switch_control::switch_complete() {
  std::lock_guard<std::mutex> lock{g_thread_dependency_bottleneck};
  is_currently_switching_ = false;
  waiting_on_thread_ = nullptr;
}

thread_info& get_current_thread_info() { return tls_thread_info; }

} // namespace concore2full::detail
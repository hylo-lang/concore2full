#include "thread_info.h"

namespace concore2full::detail {

//! Structure holding global data needed for thread control.
struct thread_control_data {
  std::mutex dependency_bottleneck_;
};

//! A single instance of `thread_control_data`.
static thread_control_data g_thread_control_data;

//! The data associated with each thread.
thread_local thread_info tls_thread_info;

bool thread_switch_control::request_switch_to(thread_info* self, thread_info* t) {
  assert(self != t);
  std::lock_guard<std::mutex> lock{g_thread_control_data.dependency_bottleneck_};
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
  std::lock_guard<std::mutex> lock{g_thread_control_data.dependency_bottleneck_};
  is_currently_switching_ = false;
  waiting_on_thread_->switch_control_.should_switch_with_.store(nullptr, std::memory_order_relaxed);
  waiting_on_thread_ = nullptr;
}

thread_info& get_current_thread_info() { return tls_thread_info; }

} // namespace concore2full::detail
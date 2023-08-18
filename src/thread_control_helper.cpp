#include <concore2full/thread_control_helper.h>

#include <iostream>

namespace concore2full {

thread_local thread_control_helper* g_thread_control_helper_{nullptr};

struct thread_control_helper::switch_data {
  std::binary_semaphore waiting_semaphore_{0};
  detail::continuation_t exit_{nullptr};
  detail::continuation_t to_switch_to_{nullptr};
};

thread_control_helper::thread_control_helper() {
  previous_ = g_thread_control_helper_;
  g_thread_control_helper_ = this;
}
thread_control_helper::~thread_control_helper() { g_thread_control_helper_ = previous_; }

void thread_control_helper::ensure_starting_thread() {
  // Are we on the same thread?
  if (g_thread_control_helper_ == this) {
    // Good. No waiting needs to happen.
  } else {
    // Request a thread inversion. We might need to wait if our original thread is currently busy.
    auto c = detail::callcc([this](detail::continuation_t c) -> detail::continuation_t {
      switch_data local_switch_data;
      local_switch_data.exit_ = c; // The orginal thread must switch to this.
      should_switch_.store(&local_switch_data, std::memory_order_release);
      // Note: After this line, the other thread can anytime switch to the `exit_` continuation,
      // destryoing `this` pointer.

      // Block until the original thread can perform the thread switch
      local_switch_data.waiting_semaphore_.acquire();
      // Switch to the continuation provided by our original thread.
      return std::exchange(local_switch_data.to_switch_to_, nullptr);
    });
  }

  // Restore the old content of our TLS variable.
  g_thread_control_helper_ = previous_;
}
void thread_control_helper::check_for_thread_inversion() {
  auto* t = g_thread_control_helper_;
  if (t) {
    switch_data* d = t->should_switch_.load(std::memory_order_acquire);
    if (d) {
      auto c = detail::callcc([d](detail::continuation_t c) -> detail::continuation_t {
        d->to_switch_to_ = c;
        auto next_for_us = std::exchange(d->exit_, nullptr);
        d->waiting_semaphore_.release();
        return next_for_us;
      });
    }
  }
}

} // namespace concore2full
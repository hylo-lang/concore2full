#include <concore2full/thread_control_helper.h>

#include <concore2full/detail/callcc.h>

#include <semaphore>
#include <utility>

namespace concore2full {

thread_local thread_control_helper* tls_thread_control_helper_{nullptr};
thread_local thread_reclaimer* tls_thread_reclaimer_{nullptr};

struct thread_control_helper::switch_data {
  std::binary_semaphore waiting_semaphore_{0};
  detail::continuation_t exit_{nullptr};
  detail::continuation_t to_switch_to_{nullptr};
};

thread_control_helper::thread_control_helper() {
  previous_ = tls_thread_control_helper_;
  tls_thread_control_helper_ = this;
  reclaimer_addrress_ = &tls_thread_reclaimer_;
}
thread_control_helper::~thread_control_helper() { tls_thread_control_helper_ = previous_; }

thread_reclaimer* thread_control_helper::get_current_thread_reclaimer() {
  return tls_thread_reclaimer_;
}

void thread_control_helper::set_current_thread_reclaimer(thread_reclaimer* new_reclaimer) {
  tls_thread_reclaimer_ = new_reclaimer;
}

void thread_control_helper::ensure_starting_thread() {
  // Are we on the same thread?
  if (tls_thread_control_helper_ == this) {
    // Good. No waiting needs to happen.
  } else {
    // Request a thread inversion. We might need to wait if our original thread is currently busy.
    (void)detail::callcc([this](detail::continuation_t c) -> detail::continuation_t {
      switch_data local_switch_data;
      local_switch_data.exit_ = c; // The orginal thread must switch to this.
      should_switch_.store(&local_switch_data, std::memory_order_release);
      // Note: After this line, the other thread can anytime switch to the `exit_` continuation,
      // destryoing `this` pointer.

      // If this thread is controlled by a thread pool which has a reclaimer registered, tell it to
      // start reclaiming.
      auto* reclaimer = *reclaimer_addrress_;
      if (reclaimer)
        reclaimer->start_reclaiming();

      // Block until the original thread can perform the thread switch
      local_switch_data.waiting_semaphore_.acquire();
      // Switch to the continuation provided by our original thread.
      return std::exchange(local_switch_data.to_switch_to_, nullptr);
    });
  }

  // Restore the old content of our TLS variable.
  tls_thread_control_helper_ = previous_;
}
void thread_control_helper::check_for_thread_inversion() {
  auto* t = tls_thread_control_helper_;
  if (t) {
    switch_data* d = t->should_switch_.load(std::memory_order_acquire);
    if (d) {
      (void)detail::callcc([d](detail::continuation_t c) -> detail::continuation_t {
        d->to_switch_to_ = c;
        auto next_for_us = std::exchange(d->exit_, nullptr);
        d->waiting_semaphore_.release();
        return next_for_us;
      });
    }
  }
}

} // namespace concore2full
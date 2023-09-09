#include "thread_info.h"

#include <concore2full/thread_control_helper.h>
#include <concore2full/thread_reclaimer.h>

#include <concore2full/detail/callcc.h>
#include <concore2full/detail/thread_switch_helper.h>

#include <mutex>
#include <semaphore>
#include <utility>

namespace concore2full {

thread_reclaimer* thread_control_helper::get_current_thread_reclaimer() {
  return detail::tls_thread_info_.thread_reclaimer_;
}

void thread_control_helper::set_current_thread_reclaimer(thread_reclaimer* new_reclaimer) {
  detail::tls_thread_info_.thread_reclaimer_ = new_reclaimer;
}

void thread_control_helper::check_for_thread_inversion() {
  // Check if some other thread requested us to switch.
  auto* first_thread =
      detail::tls_thread_info_.switch_control_.should_switch_with_.load(std::memory_order_acquire);
  if (first_thread) {
    // The switch data will be stored on the first thread.
    (void)detail::callcc([first_thread](detail::continuation_t c) -> detail::continuation_t {
      first_thread->switch_data_.secondary_start(c);
      auto next_for_us = first_thread->switch_data_.secondary_end();

      first_thread->switch_control_.waiting_semaphore_.release();

      return next_for_us;
    });
    // The originating thread will continue this control flow.
  }
}

thread_snapshot::thread_snapshot() { original_thread_ = &detail::tls_thread_info_; }

void thread_snapshot::revert() {
  // Are we on the same thread?
  if (original_thread_ == &detail::tls_thread_info_) {
    // Good. No waiting needs to happen.
  } else {
    // Wait until we can start the switch.
    // By the time waiting is over, we may be back on the desired thread, so don't do the switch in
    // that case.
    if (wait_for_switch_start())
      perform_switch();
  }
}

bool thread_snapshot::wait_for_switch_start() {
  while (true) {
    auto* cur_thread = &detail::tls_thread_info_;
    if (cur_thread->switch_control_.request_switch_to(cur_thread, original_thread_)) {
      // We started the switch process.
      return true;
    } else {
      // We cannot switch at this moment.
      // Check if another thread requested a switch from us.
      thread_control_helper::check_for_thread_inversion();
      // Now, this function may return on a different thread; check if we still need to switch.
      if (&detail::tls_thread_info_ == original_thread_) {
        return false;
      }
      // If we need to switch, introduce a small pause, so that we don't consume the CPU while
      // spinning.
      std::this_thread::yield();
    }
  }
}

void thread_snapshot::perform_switch() {
  // Request a thread inversion. We might need to wait if our original thread is currently busy.
  (void)detail::callcc([this](detail::continuation_t c) -> detail::continuation_t {
    // Use the switch data from our thread.
    auto* switch_data = &detail::tls_thread_info_.switch_data_;
    switch_data->originator_start(c);
    // TODO: fix race condition here; multiple threads may be trying to store at the same time
    original_thread_->switch_control_.should_switch_with_.store(&detail::tls_thread_info_,
                                                                std::memory_order_release);
    // Note: After this line, the other thread can anytime switch to the `after_originator_`
    // continuation, destryoing `this` pointer.

    // If this thread is controlled by a thread pool which has a reclaimer registered, tell it to
    // start reclaiming.
    auto* reclaimer = original_thread_->thread_reclaimer_;
    if (reclaimer)
      reclaimer->start_reclaiming();

    // Block until the original thread can perform the thread switch
    detail::tls_thread_info_.switch_control_.waiting_semaphore_.acquire();

    // The thread switch is complete.
    // TODO: check for race conditions
    detail::tls_thread_info_.switch_control_.switch_complete();

    // Switch to the continuation provided by our original thread.
    return detail::tls_thread_info_.switch_data_.originator_end();
  });
  // We resume here on the original thread.
  assert(original_thread_ == &detail::tls_thread_info_);
}

} // namespace concore2full
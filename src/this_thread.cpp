#include "thread_info.h"

#include <concore2full/this_thread.h>

#include <concore2full/detail/callcc.h>

namespace concore2full::this_thread {

thread_reclaimer* get_thread_reclaimer() {
  return detail::get_current_thread_info().thread_reclaimer_;
}

void set_thread_reclaimer(thread_reclaimer* new_reclaimer) {
  detail::get_current_thread_info().thread_reclaimer_ = new_reclaimer;
}

void inversion_checkpoint() {
  profiling::zone zone{CURRENT_LOCATION()};
  // Check if some other thread requested us to switch.
  auto& cur_thread = detail::get_current_thread_info();
  auto* first_thread =
      cur_thread.switch_control_.should_switch_with_.load(std::memory_order_acquire);
  if (first_thread) {
    // Now that we started the thread inversion, we can reset the request atomic.
    // Note: by construction, we should not request this thread to switch twice at the same time.
    cur_thread.switch_control_.should_switch_with_.store(nullptr, std::memory_order_relaxed);

    // The switch data will be stored on the first thread.
    (void)detail::callcc([first_thread](detail::continuation_t c) -> detail::continuation_t {
      // Switch the data for this thread.
      first_thread->target_ = c;
      auto next_for_us = first_thread->originator_;

      // Tell the originator thread that it can continue.
      first_thread->switch_control_.waiting_semaphore_.release();

      // Continue with the originator's control-flow.
      return next_for_us;
    });
    // The originating thread will continue this control flow.
  }
}

} // namespace concore2full::this_thread

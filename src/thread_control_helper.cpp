#include "thread_info.h"

#include <concore2full/thread_control_helper.h>

#include <concore2full/detail/callcc.h>

namespace concore2full {

thread_reclaimer* thread_control_helper::get_current_thread_reclaimer() {
  return detail::get_current_thread_info().thread_reclaimer_;
}

void thread_control_helper::set_current_thread_reclaimer(thread_reclaimer* new_reclaimer) {
  detail::get_current_thread_info().thread_reclaimer_ = new_reclaimer;
}

void thread_control_helper::check_for_thread_inversion() {
  // Check if some other thread requested us to switch.
  auto* first_thread = detail::get_current_thread_info().switch_control_.should_switch_with_.load(
      std::memory_order_acquire);
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

} // namespace concore2full
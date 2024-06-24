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
  detail::check_for_thread_switch();
}

} // namespace concore2full::this_thread

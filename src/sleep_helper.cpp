#include "concore2full/detail/sleep_helper.h"
#include "concore2full/profiling.h"

#include "thread_info.h"

namespace concore2full::detail {

wakeup_token::wakeup_token() : thread_(nullptr) {}

void wakeup_token::notify() {
  if (thread_)
    wake_up(*thread_);
}

void wakeup_token::invalidate() { thread_ = nullptr; }

sleep_helper::sleep_helper()
    : current_thread_(get_current_thread_info()), sleep_id_(prepare_sleep(current_thread_)) {}

void sleep_helper::sleep() {
  concore2full::detail::check_for_thread_switch();
  concore2full::detail::sleep(current_thread_, sleep_id_);
}

wakeup_token sleep_helper::get_wakeup_token() {
  wakeup_token token;
  token.thread_ = &current_thread_;
  return token;
}

} // namespace concore2full::detail
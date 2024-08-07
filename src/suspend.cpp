#include "concore2full/suspend.h"
#include "concore2full/global_thread_pool.h"

namespace concore2full {
void suspend_token::notify() { stop_source_.request_stop(); }

void suspend(suspend_token& token) {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  if (token.stop_source_.stop_requested())
    return;
  global_thread_pool().offer_help_until(token.stop_source_.get_token());
}

} // namespace concore2full
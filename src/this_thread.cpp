#include "thread_info.h"

#include <concore2full/this_thread.h>

#include <concore2full/detail/callcc.h>

namespace concore2full::this_thread {

void inversion_checkpoint() {
  profiling::zone zone{CURRENT_LOCATION()};
  detail::check_for_thread_switch();
}

} // namespace concore2full::this_thread

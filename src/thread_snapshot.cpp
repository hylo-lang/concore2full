#include <concore2full/thread_snapshot.h>

#include "thread_info.h"

namespace concore2full {

thread_snapshot::thread_snapshot() { original_thread_ = &detail::get_current_thread_info(); }

void thread_snapshot::revert() {
  // Are we on the same thread?
  if (original_thread_ == &detail::get_current_thread_info()) {
    // Good. No waiting needs to happen.
  } else {
    switch_to(original_thread_);
  }
}

} // namespace concore2full
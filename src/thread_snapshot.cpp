#include <concore2full/this_thread.h>
#include <concore2full/thread_reclaimer.h>
#include <concore2full/thread_snapshot.h>

#include <concore2full/detail/callcc.h>

#include "thread_info.h"

namespace concore2full {

thread_snapshot::thread_snapshot() { original_thread_ = &detail::get_current_thread_info(); }

void thread_snapshot::revert() {
  // Are we on the same thread?
  if (original_thread_ == &detail::get_current_thread_info()) {
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
    auto* cur_thread = &detail::get_current_thread_info();
    if (cur_thread->switch_control_.request_switch_to(cur_thread, original_thread_)) {
      // We started the switch process.
      return true;
    } else {
      // We cannot switch at this moment.
      // Check if another thread requested a switch from us.
      this_thread::inversion_checkpoint();
      // Now, this function may return on a different thread; check if we still need to switch.
      if (&detail::get_current_thread_info() == original_thread_) {
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
    auto& cur_thread = detail::get_current_thread_info();
    concore2full_store_thread_suspension(&cur_thread.originator_, c);
    // NOTE: theoretically there is a race condition here: two threads might request to switch back
    // to the same thred. That is, two control-flows will desire to continue on the same thread at
    // the same time. Because of the structured way we allow concurrency, this shouldn't happen.
    auto old = original_thread_->switch_control_.should_switch_with_.exchange(&cur_thread);
    assert(old == nullptr);
    (void)old;
    // Note: After this line, the other thread can anytime continue with the control flow,
    // destroying `this` pointer.

    // If this thread is controlled by a thread pool which has a reclaimer registered, tell it to
    // start reclaiming.
    auto* reclaimer = original_thread_->thread_reclaimer_;
    if (reclaimer)
      reclaimer->start_reclaiming();

    // Block until the original thread can perform the thread switch
    cur_thread.switch_control_.waiting_semaphore_.acquire();

    // The thread switch is complete.
    cur_thread.switch_control_.switch_complete();

    // Switch to the continuation provided by our original thread.
    return concore2full_use_thread_suspension(&cur_thread.target_);
  });
  // We resume here on the original thread.
  assert(original_thread_ == &detail::get_current_thread_info());
}

} // namespace concore2full
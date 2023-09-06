#pragma once

#include "core_types.h"

namespace concore2full {
class thread_reclaimer;
}

namespace concore2full::detail {

/**
 * @brief Helper class to store the data needed for a thread switch.
 *
 * Having two control-flows, we need to switch the threads behind them. Another way of looking at
 * this, is that we are switching the control-flows for the two threads.
 *
 * To properly perform this operation we need to switch the continuations between the two threads
 * and the thread reclaimer objects that were set for the two threads.
 *
 * The switch process will be initiated by the "originator", and the switch will happen with a
 * "secondary" thread/control-flow.
 *
 * Both control-flows will call this when they enter the switch process, and when they need to leave
 * the switch process. The flow will look like:
 *  - originator start switching
 *  - secondary starts switching
 *  - secondary ends switching
 *  - originator ends switching
 *
 * @sa thread_reclaimer
 */
class thread_switch_helper {
public:
  //! Called by the originator control-flow to start the switch.
  void originator_start(detail::continuation_t c) {
    after_originator_ = c;
    originator_reclaimer_ = thread_control_helper::get_current_thread_reclaimer();
  }
  //! Called by the originator control-flow to finish the switch.
  detail::continuation_t originator_end() {
    thread_control_helper::set_current_thread_reclaimer(secondary_reclaimer_);
    return std::exchange(after_secondary_, nullptr);
  }

  //! Called by the secondary control-flow when entering the switch process.
  void secondary_start(detail::continuation_t c) {
    after_secondary_ = c;
    secondary_reclaimer_ = thread_control_helper::get_current_thread_reclaimer();
  }
  //! Called by the secondary control-flow when exiting the switch process.
  detail::continuation_t secondary_end() {
    thread_control_helper::set_current_thread_reclaimer(originator_reclaimer_);
    return std::exchange(after_originator_, nullptr);
  }

private:
  //! The continuation for the originator control-flow; will be continued by the secondary thread.
  detail::continuation_t after_originator_{nullptr};
  //! The continuation for the other control-flow; the thread that originates the switch will
  //! continue this.
  detail::continuation_t after_secondary_{nullptr};
  //! The thread reclaimer that was on the original thread before the switch.
  thread_reclaimer* originator_reclaimer_{nullptr};
  //! The thread reclaimer that was on the other thread before the switch.
  thread_reclaimer* secondary_reclaimer_{nullptr};
};

} // namespace concore2full::detail

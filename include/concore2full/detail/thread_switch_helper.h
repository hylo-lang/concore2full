#pragma once

#include "core_types.h"

#include "concore2full/c/thread_switch.h"
#include "concore2full/this_thread.h"

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
 * the switch process. Both control-flows should call the `_start` methods before any of the `_end`
 * methods.
 *
 * @sa thread_reclaimer
 */
class thread_switch_helper {
public:
  //! Called by the originator control-flow to start the switch.
  void originator_start(detail::continuation_t c) {
    switch_data_.originator_.context_ = c;
    switch_data_.originator_.thread_reclaimer_ = this_thread::get_thread_reclaimer();
  }
  //! Called by the originator control-flow to finish the switch.
  detail::continuation_t originator_end() {
    this_thread::set_thread_reclaimer(static_cast<thread_reclaimer*>(
        atomic_load_explicit(&switch_data_.target_.thread_reclaimer_, std::memory_order_relaxed)));
    return atomic_exchange_explicit(&switch_data_.target_.context_, nullptr,
                                    std::memory_order_relaxed);
  }

  //! Called by the secondary control-flow when entering the switch process.
  void secondary_start(detail::continuation_t c) {
    switch_data_.target_.context_ = c;
    switch_data_.target_.thread_reclaimer_ = this_thread::get_thread_reclaimer();
  }
  //! Called by the secondary control-flow when exiting the switch process.
  detail::continuation_t secondary_end() {
    this_thread::set_thread_reclaimer(static_cast<thread_reclaimer*>(atomic_load_explicit(
        &switch_data_.originator_.thread_reclaimer_, std::memory_order_relaxed)));
    return atomic_exchange_explicit(&switch_data_.originator_.context_, nullptr,
                                    std::memory_order_relaxed);
  }

private:
  //! The data used to switch threads between control-flows.
  concore2full_thread_switch_data switch_data_{{nullptr, nullptr}, {nullptr, nullptr}};
};

} // namespace concore2full::detail

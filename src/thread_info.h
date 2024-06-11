#pragma once

#include "concore2full/detail/core_types.h"

#include <semaphore>

namespace concore2full {
class thread_reclaimer;
}

namespace concore2full::detail {

struct thread_info;

//! Data used for switching control flows between threads.
struct thread_switch_control {
  //! True if this thread started to switch control flows with another thread.
  //! Will be set only for the originating thread.
  bool is_currently_switching_{false};
  //! The thread that the originator thread is switching control flow with.
  thread_info* waiting_on_thread_{nullptr};

  //! Semaphore used to wait for the other thread to enter switching mode.
  std::binary_semaphore waiting_semaphore_{0};
  //! Indicates if this thread should join the switch process initiated by the value stored in here.
  std::atomic<thread_info*> should_switch_with_{nullptr};

  /**
   * @brief Mark the beginning of switching the current control flow from `self` thread to `t`.
   * @param self  The current thread, from which we initiate the control flow switch
   * @param t     The desired thread on which the originating control flow needs to switch to.
   * @return `true` if the thread switch is started; `false` if we cannot start the switch at this
   * point.
   */
  bool request_switch_to(thread_info* self, thread_info* t);
  //! Mark that the control flow switch was complete.
  void switch_complete();
};

//! Describes the data associated for each thread, used for controlling the swiching of control
//! flows.
struct thread_info {
  //! The object that needs to be notified when we want to switch control flow with another thread.
  thread_reclaimer* thread_reclaimer_{nullptr};
  //! The data used for switching control flow with other threads.
  thread_switch_control switch_control_;
  //! The thread that is originates the switch.
  continuation_t originator_{};
  //! The target thread for the switch.
  continuation_t target_{};
};

//! Get the data associated with the current thread.
thread_info& get_current_thread_info();

} // namespace concore2full::detail
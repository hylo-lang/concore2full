#pragma once

#include "concore2full/detail/catomic.h"
#include "concore2full/detail/core_types.h"
#include "concore2full/detail/thread_suspension.h"

#include <semaphore>

namespace concore2full::detail {

struct thread_info;

//! Describes the data associated for each thread, used for controlling the swiching of control
//! flows.
struct thread_info {
  //! The ID of the thread.
  std::thread::id thread_id_{};

  //! Indicates if this thread should join the switch process initiated by the value stored in here.
  std::atomic<thread_info*> should_switch_with_{nullptr};

  //! Indicates that this thread is in a switching process.
  std::atomic<bool> is_currently_switching_{false};
  //! The continuation that this thread is switching to.
  std::atomic<continuation_t> switching_to_{nullptr};

  //! Atomic variable used for sleeping and waking up the thread.
  std::atomic<uint32_t> sleeping_counter_{0};
};

//! Get the data associated with the current thread.
thread_info& get_current_thread_info();

//! Start the process of switching control flows between the current thread and `target`.
void switch_to(thread_info* target);

//! Check if the current thread is requested to perform a switch, and perform the switch if needed.
void check_for_thread_switch();

//! Prepare `thread` to go to sleep; obtains a sleep ID that is used to ensure that we can wake the
//! thread right before sleeping.
//!
//! The returned sleep ID needs to be passed to `sleep()`. Any call to `wake_up()` between this call
//! and the corresponding `sleep()` call is guaranteed to wake up the thread.
uint32_t prepare_sleep(thread_info& thread);

//! Puts `thread` to sleep until it is woken up.
void sleep(thread_info& thread, uint32_t sleep_id);

//! Wakes up `thread`.
void wake_up(thread_info& thread);

} // namespace concore2full::detail
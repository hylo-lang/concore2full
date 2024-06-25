#pragma once

#include <stdint.h>

namespace concore2full::detail {

struct thread_info;
struct sleep_helper;

//! A token that can wake up a thread.
//!
//! This token is created by the `sleep_helper` class. An invalid token can be created with the
//! default constructor.
//!
//! If the token is invalid, calling `wakeup()` will have no effect.
//! @see sleep_helper
struct wakeup_token {
  //! Creates an invalid token.
  wakeup_token();

  //! Wakes up the thread for which this token was created.
  //! Has no effect if the token is invalid.
  void notify();

  //! Transform this into an invalid token.
  void invalidate();

private:
  friend struct sleep_helper;
  thread_info* thread_;
};

/**
 * @brief Helper class to put the current thread to sleep.
 *
 * We define a space between the constructor call and the `sleep()` call, to check any conditions
 * before we put the thread to sleep. Any thread wakeup calls between the constructor and the
 * `sleep()` will be guarantee to wake up the thread.
 *
 * By contrast, calls to wakeup for the thread that happen before this constructor won't wake up the
 * thread.
 *
 * The users shall put between the constructor and the call to `sleep` conditions that may prevent
 * the thread to go to sleep.
 *
 * Note: before going to sleep, this will also check for thread inversions.
 */
struct sleep_helper {
  sleep_helper();
  ~sleep_helper() = default;

  void sleep();

  //! Get a token that can wake up the thread that we are putting to sleep.
  wakeup_token get_wakeup_token();

private:
  //! The thread that we want to put to sleep.
  thread_info& current_thread_;
  //! The ID of the sleep operation; used to coordinate the constructor and the sleep operation.
  uint32_t sleep_id_;
};

} // namespace concore2full::detail
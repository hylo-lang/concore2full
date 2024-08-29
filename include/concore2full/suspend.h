#pragma once

#include <stop_token>

namespace concore2full {

//! Token used for waking up a suspended execution.
struct suspend_token {
  //! Wake up the execution that is suspended on this token.
  //!
  //! This can be called before the thread actually suspends; in this case, the thread will not be
  //! actually suspended.
  void notify();

private:
  //! Stop source that signals when the execution should be woken up.
  std::stop_source stop_source_;

  friend void suspend(suspend_token& token);
  friend void suspend_quick_resume(suspend_token& token);
};

//! Suspends the current execution until `token` is notified.
//!
//! If the thread pool is executing something, then the suspend may not return immediatelly.
//! This will allow the thread pool to use the current thread for other activities.
void suspend(suspend_token& token);

//! Suspends the current execution until `token` is notified; when notified, the execution will
//! continue asap.
//!
//! If the thread pool is executing something, this will not wait until the task is done; it will
//! quickly continue by spawning a new task. While suspended, this will allow the thread pool to use
//! the current thread for other activities.
void suspend_quick_resume(suspend_token& token);

} // namespace concore2full
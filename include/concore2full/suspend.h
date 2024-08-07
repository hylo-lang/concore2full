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
};

//! Suspends the current execution until `token` is notified.
//!
//! This will allow the thread pool to use the current thread for other activities.
void suspend(suspend_token& token);

} // namespace concore2full
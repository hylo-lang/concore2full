#pragma once

#include "atomic_wrapper.h"
#include "catomic.h"

#include <context_core_api.h>

namespace concore2full {
class thread_reclaimer;
}

namespace concore2full::detail {

//! Holds the data for a thread suspension point.
struct thread_suspension {
  thread_suspension() = default;

  //! Returns the continuation after the suspension point.
  const catomic<context_core_api_fcontext_t>& continuation() { return continuation_; }

  //! Stores data about the current suspension point (`c` and current thread reclaimer) into `data`
  //! (relaxed atomic mode).
  void store_relaxed(context_core_api_fcontext_t c);
  //! Stores data about the current suspension point (`c` and current thread reclaimer) into `data`
  //! (release atomic mode).
  void store_release(context_core_api_fcontext_t c);

  //! Applies the thread reclaimer and return the continuation from `this`.
  //!
  //! To be used when trying to resume a suspension point, possible from a different thread.
  //! Use acquire mode to access the continuation.
  context_core_api_fcontext_t use_thread_suspension_acquire();

  //! Applies the thread reclaimer and return the continuation from `this`.
  //!
  //! To be used when trying to resume a suspension point, possible from a different thread.
  //! Use relaxed mode to access the continuation.
  context_core_api_fcontext_t use_thread_suspension_relaxed();

private:
  //! The continuation after the suspension point.
  concore2full::detail::catomic<context_core_api_fcontext_t> continuation_;

  //! The thread reclaimer used at the point of suspension.
  thread_reclaimer* thread_reclaimer_{nullptr};
};

} // namespace concore2full::detail

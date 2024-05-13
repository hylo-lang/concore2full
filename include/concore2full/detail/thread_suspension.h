#ifndef __CONCORE2FULL_THREAD_SUSPENSION_H__
#define __CONCORE2FULL_THREAD_SUSPENSION_H__

#include "atomic_wrapper.h"

#include <context_core_api.h>

#ifdef __cplusplus
extern "C" {
#endif

//! Holds the data for a thread suspension point.
struct concore2full_thread_suspension {
  //! The continuation after the suspension point.
  context_core_api_fcontext_t continuation_;

  //! The thread reclaimer used at the point of suspension.
  void* thread_reclaimer_;
};

//! Same as `concore2full_thread_suspension`, but adds the ability to synchronize with the
//! continuation object.
struct concore2full_thread_suspension_sync {
  //! The continuation after the suspension point.
  CONCORE2FULL_ATOMIC(context_core_api_fcontext_t) continuation_;

  //! The thread reclaimer used at the point of suspension.
  void* thread_reclaimer_;
};

//! Stores data about the current suspension point (`c` and current thread reclaimer) into `data`.
void concore2full_store_thread_suspension(struct concore2full_thread_suspension* data,
                                          context_core_api_fcontext_t c);
//! Stores data about the current suspension point (`c` and current thread reclaimer) into `data`.
void concore2full_store_thread_suspension_release(struct concore2full_thread_suspension_sync* data,
                                                  context_core_api_fcontext_t c);

//! Applies the thread reclaimer from `data` and return the continuation from `data`.
//!
//! To be used when trying to resume a suspension point, possible from a different thread.
context_core_api_fcontext_t
concore2full_use_thread_suspension(struct concore2full_thread_suspension* data);
//! Applies the thread reclaimer from `data` and return the continuation from `data`.
//!
//! To be used when trying to resume a suspension point, possible from a different thread.
context_core_api_fcontext_t
concore2full_use_thread_suspension_acquire(struct concore2full_thread_suspension_sync* data);

//! Applies the thread reclaimer from `data` and return the continuation from `data`.
//!
//! To be used when trying to resume a suspension point, possible from a different thread.
//! Use relaxed mode to access the continuation.
context_core_api_fcontext_t
concore2full_use_thread_suspension_relaxed(struct concore2full_thread_suspension_sync* data);

#ifdef __cplusplus
}
#endif

#endif

#ifndef __CONCORE2FULL_THREAD_SWITCH_H__
#define __CONCORE2FULL_THREAD_SWITCH_H__

#include <context_core_api.h>

#ifdef __cplusplus
#include <atomic>
#define CONCORE2FULL_ATOMIC(x) std::atomic<x>
#else
#define CONCORE2FULL_ATOMIC(x) volatile x
#endif

#ifdef __cplusplus
extern "C" {
#endif

//! Describes the state of an OS thread.
struct concore2full_thread_data {

  //! An execution context running on the OS thread.
  CONCORE2FULL_ATOMIC(context_core_api_fcontext_t) context_;

  //! Data used to wake-up the thread for performing a thread reclamation.
  CONCORE2FULL_ATOMIC(void*) thread_reclaimer_;
};

//! Data used to switch threads between control-flows.
struct concore2full_thread_switch_data {

  //! The thread that is originates the switch.
  struct concore2full_thread_data originator_;

  //! The target thread for the switch.
  struct concore2full_thread_data target_;
};

//! Stores data about the current (`c` and current thread reclaimer) thread into `data`.
void concore2full_store_thread_data_relaxed(struct concore2full_thread_data* data,
                                            context_core_api_fcontext_t c);
void concore2full_store_thread_data_release(struct concore2full_thread_data* data,
                                            context_core_api_fcontext_t c);

//! Switches the current thread reclaimer to the one hold by `data` and returns the continuation we
//! should be switching to.
context_core_api_fcontext_t
concore2full_exchange_thread_with(struct concore2full_thread_data* data);

#ifdef __cplusplus
}
#endif

#endif

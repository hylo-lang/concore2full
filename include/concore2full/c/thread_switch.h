#ifndef __CONCORE2FULL_THREAD_SWITCH_H__
#define __CONCORE2FULL_THREAD_SWITCH_H__

#include <context_core_api.h>

#ifdef __cplusplus
extern "C" {
#endif

//! Describes the state of an OS thread.
struct concore2full_thread_data {

  //! An execution context running on the OS thread.
  context_core_api_fcontext_t context_;

  //! Data used to wake-up the thread for performing a thread reclamation.
  void* thread_reclaimer_;
};

//! Data used to switch threads between control-flows.
struct concore2full_thread_switch_data {

  //! The thread that is originates the switch.
  struct concore2full_thread_data originator_;

  //! The target thread for the switch.
  struct concore2full_thread_data target_;
};

#ifdef __cplusplus
}
#endif

#endif

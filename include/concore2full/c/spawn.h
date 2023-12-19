#ifndef __CONCORE2FULL_SPAWN_H__
#define __CONCORE2FULL_SPAWN_H__

#include "concore2full/c/task.h"
#include "concore2full/c/thread_switch.h"
#include <context_core_api.h>

#ifdef __cplusplus
extern "C" {
#endif

struct concore2full_spawn_frame;

//! Type of a user function to be executed on `spawn`.
typedef void (*concore2full_spawn_function_t)(struct concore2full_spawn_frame*);

//! Data needed to perform a `spawn` operation.
struct concore2full_spawn_frame {

  //! Describes how to view the spawn data as a task.
  struct concore2full_task task_;

  //! The state of the computation, with respect to reaching the await point.
  _Atomic(int) sync_state_;

  //! Indicates that the async processing has started (continuation is set).
  _Atomic(int) async_started_;

  //! Data used to switch threads between control-flows.
  struct concore2full_thread_switch_data switch_data_;

  //! The user function to be called to execute the async work.
  concore2full_spawn_function_t user_function_;
};

void concore2full_initialize(struct concore2full_spawn_frame* frame,
                             concore2full_spawn_function_t user_function);

#ifdef __cplusplus
}
#endif

#endif

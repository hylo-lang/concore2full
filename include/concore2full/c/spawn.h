#ifndef __CONCORE2FULL_SPAWN_H__
#define __CONCORE2FULL_SPAWN_H__

#include <context_core_api.h>

#include <stdatomic.h>

#ifdef __cplusplus
extern "C" {
#endif

struct concore2full_task_base;
struct concore2full_spawn_data;

//! Type of a function that can be executed as a task.
typedef void (*concore2full_task_function_t)(struct concore2full_task_base* task, int worker_index);

//! A task that can be executed.
struct concore2full_task_base {

  //! The function to be called to execute the task.
  concore2full_task_function_t task_function_;

  //! Pointer to the next element in the list of task; implementation details.
  struct concore2full_task_base* next_;
};

//! Type of a user function to be executed on `spawn`.
typedef void (*concore2full_spawn_function_t)(struct concore2full_spawn_data* data);

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

//! Data needed to perform a `spawn` operation.
struct concore2full_spawn_data {

  //! Describes how to view the spawn data as a task.
  struct concore2full_task_base task_;

  //! The state of the computation, with respect to reaching the await point.
  atomic_int sync_state_;

  //! Indicates that the async processing has started (continuation is set).
  atomic_bool async_started_;

  //! Data used to switch threads between control-flows.
  struct concore2full_thread_switch_data switch_data_;

  //! The user function to be called to execute the async work.
  concore2full_spawn_function_t user_function_;
};

#ifdef __cplusplus
}
#endif

#endif

#ifndef __CONCORE2FULL_BULK_SPAWN_H__
#define __CONCORE2FULL_BULK_SPAWN_H__

#include "concore2full/c/task.h"
#include "concore2full/c/thread_switch.h"
#include <context_core_api.h>

#ifdef __cplusplus
#include <atomic>
#define CONCORE2FULL_ATOMIC(x) std::atomic<x>
#else
#define CONCORE2FULL_ATOMIC(x) volatile x
#endif

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct concore2full_bulk_spawn_frame;

//! Type of a user function to be executed on `bulk_spawn`.
typedef void (*concore2full_bulk_spawn_function_t)(struct concore2full_bulk_spawn_frame*, int);

struct concore2full_bulk_spawn_task;

//! Data needed to perform a `bulk_spawn` operation.
//! We need dynamic memory for this, so this will be allocated on the heap.
struct concore2full_bulk_spawn_frame {

  //! The number of work item for the bulk operation.
  int count_;

  //! The number of started tasks.
  CONCORE2FULL_ATOMIC(int) started_tasks_;

  //! The number of completed tasks.
  CONCORE2FULL_ATOMIC(int) completed_tasks_;
  //! The number of finalized tasks.
  CONCORE2FULL_ATOMIC(int) finalized_tasks_;

  //! The user function to be called to execute the async work.
  concore2full_bulk_spawn_function_t user_function_;

  //! The tasks for each work item.
  struct concore2full_bulk_spawn_task* tasks_;

  //! The data needed to interact with each thread of execution; at position `_count + 1` we store
  //! the information about the thread doing the await.
  struct concore2full_thread_data* threads_;
};

//! Returns the full size of the `concore2full_bulk_spawn_frame` structure, given the number of work items.
size_t concore2full_frame_size(int count);

//! Asynchronously executes `f`, using the given `frame` to hold the state.
void concore2full_bulk_spawn(struct concore2full_bulk_spawn_frame* frame, int count,
                             concore2full_bulk_spawn_function_t f);
void concore2full_bulk_spawn2(struct concore2full_bulk_spawn_frame* frame, int count,
                              concore2full_bulk_spawn_function_t* f);

//! Await the async computations represented by `frame` to be finished.
void concore2full_bulk_await(struct concore2full_bulk_spawn_frame* frame);

#ifdef __cplusplus
}
#endif

#endif

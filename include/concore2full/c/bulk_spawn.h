#ifndef __CONCORE2FULL_BULK_SPAWN_H__
#define __CONCORE2FULL_BULK_SPAWN_H__

#include "concore2full/c/atomic_wrapper.h"
#include "concore2full/c/task.h"
#include "concore2full/c/thread_suspension.h"
#include <context_core_api.h>

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct concore2full_bulk_spawn_frame;

//! Type of a user function to be executed on `bulk_spawn`.
typedef void (*concore2full_bulk_spawn_function_t)(struct concore2full_bulk_spawn_frame*, uint64_t);

//! Data needed to perform a `bulk_spawn` operation.
//! The user cannot allocate this directly, so we type-erased it.
struct concore2full_bulk_spawn_frame {
  void* dummy;
};

//! Returns the full size of the `concore2full_bulk_spawn_frame` structure, given the number of work
//! items.
uint64_t concore2full_frame_size(int32_t count);

//! Asynchronously executes `f`, using the given `frame` to hold the state.
void concore2full_bulk_spawn(struct concore2full_bulk_spawn_frame* frame, int32_t count,
                             concore2full_bulk_spawn_function_t f);
void concore2full_bulk_spawn2(struct concore2full_bulk_spawn_frame* frame, int32_t* count,
                              concore2full_bulk_spawn_function_t* f);

//! Await the async computations represented by `frame` to be finished.
void concore2full_bulk_await(struct concore2full_bulk_spawn_frame* frame);

#ifdef __cplusplus
}
#endif

#endif

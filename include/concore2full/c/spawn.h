#ifndef __CONCORE2FULL_SPAWN_H__
#define __CONCORE2FULL_SPAWN_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//! Data needed to perform a `spawn` operation.
//! Must be at least the size that the implementation expects.
struct concore2full_spawn_frame {
  void* data[10];
};

//! Data needed to perform a `bulk_spawn` operation.
//! The user cannot allocate this directly, so we type-erased it.
struct concore2full_bulk_spawn_frame {
  void* dummy;
};

//! Type of a user function to be executed on `spawn`.
typedef void (*concore2full_spawn_function_t)(struct concore2full_spawn_frame*);

//! Type of a user function to be executed on `bulk_spawn`.
typedef void (*concore2full_bulk_spawn_function_t)(struct concore2full_bulk_spawn_frame*, uint64_t);

//! Asynchronously executes `f`, using the given `frame` to hold the state.
void concore2full_spawn(struct concore2full_spawn_frame* frame, concore2full_spawn_function_t f);

//! Await the async computation represented by `frame` to be finished.
void concore2full_await(struct concore2full_spawn_frame* frame);

//! Returns the full size of the `concore2full_bulk_spawn_frame` structure, given the number of work
//! items.
uint64_t concore2full_frame_size(int32_t count);

//! Asynchronously executes `f`, using the given `frame` to hold the state.
void concore2full_bulk_spawn(struct concore2full_bulk_spawn_frame* frame, int32_t count,
                             concore2full_bulk_spawn_function_t f);

//! Await the async computations represented by `frame` to be finished.
void concore2full_bulk_await(struct concore2full_bulk_spawn_frame* frame);

// TEMPORARY WORKAROUND

void concore2full_spawn2(struct concore2full_spawn_frame* frame, concore2full_spawn_function_t* f);
void concore2full_bulk_spawn2(struct concore2full_bulk_spawn_frame* frame, int32_t* count,
                              concore2full_bulk_spawn_function_t* f);

#ifdef __cplusplus
}
#endif

#endif

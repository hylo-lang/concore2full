#ifndef __CONCORE2FULL_SPAWN_H__
#define __CONCORE2FULL_SPAWN_H__

#include "concore2full/c/task.h"
#include "concore2full/c/thread_suspension.h"
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

struct concore2full_spawn_frame;

//! Type of a user function to be executed on `spawn`.
typedef void (*concore2full_spawn_function_t)(struct concore2full_spawn_frame*);

//! Data needed to perform a `spawn` operation.
struct concore2full_spawn_frame {
  void* data[10];
};

//! Asynchronously executes `f`, using the given `frame` to hold the state.
void concore2full_spawn(struct concore2full_spawn_frame* frame, concore2full_spawn_function_t f);

//! Await the async computation represented by `frame` to be finished.
void concore2full_await(struct concore2full_spawn_frame* frame);

// TEMPORARY WORKAROUND

void concore2full_spawn2(struct concore2full_spawn_frame* frame, concore2full_spawn_function_t* f);

#ifdef __cplusplus
}
#endif

#endif

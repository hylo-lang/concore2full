#pragma once

#include "concore2full/c/atomic_wrapper.h"
#include "concore2full/c/spawn.h"
#include "concore2full/c/task.h"
#include "concore2full/c/thread_suspension.h"
#include "concore2full/detail/callcc.h"
#include "concore2full/detail/value_holder.h"
#include "concore2full/this_thread.h"

#include <memory>
#include <type_traits>

namespace concore2full::detail {

//! Basic structure needed to perform a `spawn` operation.
struct spawn_frame_base {

  using interface_t = concore2full_spawn_frame;

  spawn_frame_base() = default;

  static spawn_frame_base* from_interface(interface_t* src) {
    return reinterpret_cast<spawn_frame_base*>(src);
  }
  interface_t* to_interface() { return reinterpret_cast<interface_t*>(this); }

  //! Asynchronously executes `f`.
  void spawn(concore2full_spawn_function_t f);

  //! Await the async computation started by `spawn` to be finished.
  void await();

private:
  //! Describes how to view the spawn data as a task.
  struct concore2full_task task_;

  //! The state of the computation, with respect to reaching the await point.
  CONCORE2FULL_ATOMIC(int) sync_state_;

  //! The suspension point of the originator of the spawn.
  struct concore2full_thread_suspension originator_;

  //! The suspension point of the thread that is performing the spawned work.
  struct concore2full_thread_suspension secondary_thread_;

  //! The user function to be called to execute the async work.
  concore2full_spawn_function_t user_function_;

private:
  //! Called when the spawned work is completed.
  continuation_t on_async_complete(continuation_t c);
  //! The task function that executes the spawned work.
  static void execute_spawn_task(concore2full_task* task, int) noexcept;
};

} // namespace concore2full::detail
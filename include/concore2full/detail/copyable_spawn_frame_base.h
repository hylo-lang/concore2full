#pragma once

#include "concore2full/c/spawn.h"
#include "concore2full/c/task.h"
#include "concore2full/detail/callcc.h"
#include "concore2full/detail/value_holder.h"
#include "concore2full/profiling_atomic.h"
#include "concore2full/suspend.h"
#include "concore2full/this_thread.h"

#include <memory>
#include <type_traits>

namespace concore2full::detail {

//! Basic structure needed to perform a `spawn` operation.
struct copyable_spawn_frame_base {

  using interface_t = concore2full_spawn_frame;

  copyable_spawn_frame_base() = default;

  static copyable_spawn_frame_base* from_interface(interface_t* src) {
    return reinterpret_cast<copyable_spawn_frame_base*>(src);
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
  profiling::atomic<uint32_t> sync_state_;
  //! The number of threads that reached await.
  std::atomic<uint32_t> awaiters_count_{0};

  //! The suspension point of the first thread to arrive in await.
  continuation_t first_await_;

  //! The suspension point of the thread that is performing the spawned work.
  continuation_t secondary_thread_;

  //! The user function to be called to execute the async work.
  concore2full_spawn_function_t user_function_;

  //! Token that will wake any suspended threads of execution.
  suspend_token suspend_token_;

private:
  //! Called when the spawned work is completed.
  continuation_t on_async_complete(continuation_t c);
  //! The task function that executes the spawned work.
  static void execute_spawn_task(concore2full_task* task, int) noexcept;
};

} // namespace concore2full::detail
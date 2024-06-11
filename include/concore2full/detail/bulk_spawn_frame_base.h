#pragma once

#include "concore2full/c/spawn.h"
#include "concore2full/c/task.h"
#include "concore2full/detail/catomic.h"
#include "concore2full/detail/core_types.h"
#include "concore2full/this_thread.h"

#include <memory>
#include <type_traits>

namespace concore2full::detail {

struct concore2full_bulk_spawn_task;

//! Basic structure needed to perform a *bulk spawn* operation.
struct bulk_spawn_frame_base {

  using interface_t = concore2full_bulk_spawn_frame;

  bulk_spawn_frame_base() = default;

  static bulk_spawn_frame_base* from_interface(interface_t* src) {
    return reinterpret_cast<bulk_spawn_frame_base*>(src);
  }
  interface_t* to_interface() { return reinterpret_cast<interface_t*>(this); }

  //! Returns the frame size we need for storing this object, given the number of work items.
  static uint64_t frame_size(int32_t count);

  //! Asynchronously executes `f` for indices in range [0, `count`).
  void spawn(int32_t count, concore2full_bulk_spawn_function_t f);

  //! Await the async computation started by `spawn` to be finished.
  void await();

public:
  // private:
  //! The number of work item for the bulk operation.
  uint32_t count_;

  //! The number of started tasks.
  std::atomic<uint32_t> started_tasks_;

  //! The number of completed tasks.
  std::atomic<uint32_t> completed_tasks_;
  //! The number of finalized tasks.
  std::atomic<uint32_t> finalized_tasks_;

  //! The user function to be called to execute the async work.
  concore2full_bulk_spawn_function_t user_function_;

  //! The tasks for each work item.
  concore2full_bulk_spawn_task* tasks_;

  //! The data needed to interact with each thread of execution; at position `_count + 1` we store
  //! the information about the thread doing the await.
  catomic<context_core_api_fcontext_t>* threads_;

  // More data will follow here, depending on the number of work items.

private:
  //! Called by the spawned tasks to store the continuation back to the worker pool.
  int store_worker_continuation(continuation_t c);
  //! Extract a continuation stored by a worker thread.
  catomic<context_core_api_fcontext_t>* extract_continuation();
  //! Called when a a thread finishes work and wants to exit the spawn scope.
  void finalize_thread_of_execution(bool is_last_thread);
  //! The task function that executes the async work.
  static void execute_bulk_spawn_task(concore2full_task* t, int) noexcept;
};

} // namespace concore2full::detail
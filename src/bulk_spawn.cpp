#include "concore2full/c/bulk_spawn.h"
#include "concore2full/detail/atomic_wait.h"
#include "concore2full/detail/callcc.h"
#include "concore2full/global_thread_pool.h"
#include "concore2full/profiling.h"

#include <chrono>
#include <cstring>

using concore2full::detail::callcc;
using concore2full::detail::continuation_t;

struct concore2full_bulk_spawn_task : concore2full_task {
  struct concore2full_bulk_spawn_frame* base_;
};

namespace {

using concore2full::detail::as_value;

continuation_t tombstone_continuation() { return (continuation_t)0x1; }

int store_current_continuation(concore2full_bulk_spawn_frame* frame, continuation_t c) {
  // Occupy the next thread slot.
  int cont_index = atomic_fetch_add(&frame->started_tasks_, 1);
  assert(cont_index < frame->count_);
  // Store the thread data to the proper continuation index.
  // This is different from the index of the task, as we may store the continuation out of order.
  concore2full_store_thread_suspension_release(&frame->threads_[cont_index], c);
  return cont_index;
}

concore2full_thread_suspension_sync* extract_continuation(concore2full_bulk_spawn_frame* frame) {
  while (true) {
    // Obtain the index of the slot from which we need to extract.
    int index = atomic_fetch_add(&frame->completed_tasks_, 1);
    assert(index <= frame->count_);

    auto* r = &frame->threads_[index];

    // Ensure that the thread data is properly initialized.
    // I.e., we didn't reach here before the other thread finished storing the data.
    concore2full::detail::atomic_wait(r->continuation_, [](auto c) { return c != nullptr; });

    // If we've found a valid continuation, return it.
    if (r->continuation_ != tombstone_continuation())
      return r;
    // otherwise, pick the next one.
  }
}

void finalize_thread_of_execution(concore2full_bulk_spawn_frame* frame, bool is_last_thread) {
  // Obtain the index of the slot from which we need to extract.
  int count = frame->count_;
  atomic_fetch_add(&frame->finalized_tasks_, 1);
  if (is_last_thread) {
    // Last thread needs to ensure that all other threads have finalized their maintenance work
    // before returning to the continuation after the await point.
    concore2full::detail::atomic_wait(frame->finalized_tasks_,
                                      [count](int v) { return v == count + 1; });
  }
  // After this point, the `frame` object may be destroyed (by the last thread).
}

//! The task function that executes the async work.
void execute_bulk_spawn_task(concore2full_task* t, int) noexcept {
  auto task = reinterpret_cast<concore2full_bulk_spawn_task*>(t);
  auto frame = task->base_;
  int index = (int)(task - frame->tasks_);
  (void)callcc([frame, index](continuation_t thread_cont) -> continuation_t {
    // Store the current continuation, so that other threads can extract it.
    int cont_index = store_current_continuation(frame, thread_cont);

    // Actually execute the given work.
    frame->user_function_(frame, index);

    // Extract the next free continuation data and switch to it.
    concore2full_thread_suspension_sync* cont_data = extract_continuation(frame);
    if (cont_data == &frame->threads_[cont_index]) {
      // We are finishing on the same thread that started the task.
      finalize_thread_of_execution(frame, false);
      return thread_cont;
    } else {
      // We are finishing on a different thread than the one that started the task.
      auto r = concore2full_use_thread_suspension_acquire(cont_data);
      assert(r);

      bool last_thread = cont_data == &frame->threads_[frame->count_];
      finalize_thread_of_execution(frame, last_thread);

      return r;
    }
  });
}

} // namespace

extern "C" uint64_t concore2full_frame_size(int32_t count) {
  return sizeof(concore2full_bulk_spawn_frame)                       //
         + count * sizeof(concore2full_bulk_spawn_task)              //
         + (count + 1) * sizeof(concore2full_thread_suspension_sync) //
      ;
}

void concore2full_bulk_spawn(struct concore2full_bulk_spawn_frame* frame, int32_t count,
                             concore2full_bulk_spawn_function_t f) {
  size_t size_struct = sizeof(concore2full_bulk_spawn_frame);
  size_t size_tasks = count * sizeof(concore2full_bulk_spawn_task);
  char* p = reinterpret_cast<char*>(frame);
  frame->tasks_ = reinterpret_cast<concore2full_bulk_spawn_task*>(p + size_struct);
  frame->threads_ =
      reinterpret_cast<concore2full_thread_suspension_sync*>(p + size_struct + size_tasks);

  frame->count_ = count;
  frame->started_tasks_ = 0;
  frame->completed_tasks_ = 0;
  frame->finalized_tasks_ = 0;
  frame->user_function_ = f;
  for (int i = 0; i < count; i++) {
    frame->tasks_[i].task_function_ = &execute_bulk_spawn_task;
    frame->tasks_[i].next_ = nullptr;
    frame->tasks_[i].base_ = frame;
  }
  for (int i = 0; i < count + 1; i++) {
    frame->threads_[i].continuation_ = nullptr;
    frame->threads_[i].thread_reclaimer_ = nullptr;
  }

  concore2full::global_thread_pool().enqueue_bulk(frame->tasks_, count);
}

void concore2full_bulk_spawn2(struct concore2full_bulk_spawn_frame* frame, int32_t* count,
                              concore2full_bulk_spawn_function_t* f) {
  concore2full_bulk_spawn(frame, *count, *f);
}

void concore2full_bulk_await(struct concore2full_bulk_spawn_frame* frame) {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};

  // If all the workers have finished, we can return directly.
  uint64_t completed = atomic_load_explicit(&frame->completed_tasks_, std::memory_order_acquire);
  if (completed == uint64_t(frame->count_))
    return;

  // Try to execute as much as possible inplace.
  for (uint32_t i = 0; i < frame->count_; i++) {
    if (concore2full::global_thread_pool().extract_task(&frame->tasks_[i])) {
      // Occupy one slot in the completed tasks.
      store_current_continuation(frame, tombstone_continuation());

      {
        concore2full::profiling::zone z{CURRENT_LOCATION_N("execute inplace")};
        // Actually execute the given work.
        frame->user_function_(frame, i);
      }

      finalize_thread_of_execution(frame, false);
    }
  }

  // We may need to switching threads, so we need a continuation.
  auto c = callcc([frame](continuation_t await_cc) -> continuation_t {
    // Store the current continuation, so that other threads can extract it.
    // We always store the continuation at `count_` position, so that this is the last one to be
    // extracted.
    concore2full::profiling::zone await_zone{CURRENT_LOCATION_N("await")};
    await_zone.set_param("ctx", (uint64_t)await_cc);
    concore2full_store_thread_suspension_release(&frame->threads_[frame->count_], await_cc);

    // Extract the next free continuation data and switch to it.
    auto c1 = extract_continuation(frame);
    auto r = concore2full_use_thread_suspension_relaxed(c1);
    assert(r);

    bool last_thread = c1 == &frame->threads_[frame->count_];
    finalize_thread_of_execution(frame, last_thread);

    return r;
  });
  (void)c;
  // This point will be executed by the thread that finishes last.
}

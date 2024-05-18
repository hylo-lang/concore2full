#include "concore2full/detail/bulk_spawn_frame_base.h"
#include "concore2full/c/spawn.h"
#include "concore2full/detail/atomic_wait.h"
#include "concore2full/detail/callcc.h"
#include "concore2full/detail/thread_suspension.h"
#include "concore2full/global_thread_pool.h"
#include "concore2full/profiling.h"

#include <chrono>
#include <cstring>

using concore2full::detail::bulk_spawn_frame_base;
using concore2full::detail::callcc;
using concore2full::detail::continuation_t;

namespace concore2full::detail {

struct concore2full_bulk_spawn_task : concore2full_task {
  bulk_spawn_frame_base* base_;
};

} // namespace concore2full::detail

namespace {

continuation_t tombstone_continuation() { return (continuation_t)0x1; }

} // namespace

int bulk_spawn_frame_base::store_worker_continuation(continuation_t c) {
  // Occupy the next thread slot.
  int cont_index = atomic_fetch_add(&started_tasks_, 1);
  assert(cont_index < count_);
  // Store the thread data to the proper continuation index.
  // This is different from the index of the task, as we may store the continuation out of order.
  threads_[cont_index].store_release(c);
  return cont_index;
}

concore2full::detail::thread_suspension* bulk_spawn_frame_base::extract_continuation() {
  while (true) {
    // Obtain the index of the slot from which we need to extract.
    int index = atomic_fetch_add(&completed_tasks_, 1);
    assert(index <= count_);

    auto* r = &threads_[index];

    // Ensure that the thread data is properly initialized.
    // I.e., we didn't reach here before the other thread finished storing the data.
    concore2full::detail::atomic_wait(r->continuation(), [](auto c) { return c != nullptr; });

    // If we've found a valid continuation, return it.
    if (r->continuation().load(std::memory_order_relaxed) != tombstone_continuation())
      return r;
    // otherwise, pick the next one.
  }
}

void bulk_spawn_frame_base::finalize_thread_of_execution(bool is_last_thread) {
  // Obtain the index of the slot from which we need to extract.
  int count = count_;
  atomic_fetch_add(&finalized_tasks_, 1);
  if (is_last_thread) {
    // Last thread needs to ensure that all other threads have finalized their maintenance work
    // before returning to the continuation after the await point.
    concore2full::detail::atomic_wait(finalized_tasks_, [count](int v) { return v == count + 1; });
  }
  // After this point, the `this` object may be destroyed (by the last thread).
}

void bulk_spawn_frame_base::execute_bulk_spawn_task(concore2full_task* t, int) noexcept {
  auto task = reinterpret_cast<concore2full_bulk_spawn_task*>(t);
  auto frame = task->base_;
  int index = (int)(task - frame->tasks_);
  (void)callcc([frame, index](continuation_t thread_cont) -> continuation_t {
    // Store the current continuation, so that other threads can extract it.
    int cont_index = frame->store_worker_continuation(thread_cont);

    // Actually execute the given work.
    frame->user_function_(frame->to_interface(), index);

    // Extract the next free continuation data and switch to it.
    thread_suspension* cont_data = frame->extract_continuation();
    if (cont_data == &frame->threads_[cont_index]) {
      // We are finishing on the same thread that started the task.
      frame->finalize_thread_of_execution(false);
      return thread_cont;
    } else {
      // We are finishing on a different thread than the one that started the task.
      auto r = cont_data->use_thread_suspension_acquire();
      assert(r);

      bool last_thread = cont_data == &frame->threads_[frame->count_];
      frame->finalize_thread_of_execution(last_thread);

      return r;
    }
  });
}

uint64_t bulk_spawn_frame_base::frame_size(int32_t count) {
  return sizeof(bulk_spawn_frame_base)                  //
         + count * sizeof(concore2full_bulk_spawn_task) //
         + (count + 1) * sizeof(thread_suspension)      //
      ;
}

void bulk_spawn_frame_base::spawn(int32_t count, concore2full_bulk_spawn_function_t f) {
  size_t size_struct = sizeof(bulk_spawn_frame_base);
  size_t size_tasks = count * sizeof(concore2full_bulk_spawn_task);
  char* p = reinterpret_cast<char*>(this);
  tasks_ = reinterpret_cast<concore2full_bulk_spawn_task*>(p + size_struct);
  threads_ = reinterpret_cast<thread_suspension*>(p + size_struct + size_tasks);

  count_ = count;
  started_tasks_ = 0;
  completed_tasks_ = 0;
  finalized_tasks_ = 0;
  user_function_ = f;
  for (int i = 0; i < count; i++) {
    tasks_[i].task_function_ = &execute_bulk_spawn_task;
    tasks_[i].next_ = nullptr;
    tasks_[i].base_ = this;
  }
  for (int i = 0; i < count + 1; i++) {
    threads_[i] = thread_suspension{};
  }

  concore2full::global_thread_pool().enqueue_bulk(tasks_, count);
}

void bulk_spawn_frame_base::await() {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};

  // If all the workers have finished, we can return directly.
  uint64_t completed = atomic_load_explicit(&completed_tasks_, std::memory_order_acquire);
  if (completed == uint64_t(count_))
    return;

  // Try to execute as much as possible inplace.
  for (uint32_t i = 0; i < count_; i++) {
    if (concore2full::global_thread_pool().extract_task(&tasks_[i])) {
      // Occupy one slot in the completed tasks.
      store_worker_continuation(tombstone_continuation());

      {
        concore2full::profiling::zone z{CURRENT_LOCATION_N("execute inplace")};
        // Actually execute the given work.
        user_function_(this->to_interface(), i);
      }

      finalize_thread_of_execution(false);
    }
  }

  // We may need to switching threads, so we need a continuation.
  auto c = callcc([this](continuation_t await_cc) -> continuation_t {
    // Store the current continuation, so that other threads can extract it.
    // We always store the continuation at `count_` position, so that this is the last one to be
    // extracted.
    concore2full::profiling::zone await_zone{CURRENT_LOCATION_N("await")};
    await_zone.set_param("ctx", (uint64_t)await_cc);
    threads_[count_].store_release(await_cc);

    // Extract the next free continuation data and switch to it.
    auto c1 = extract_continuation();
    auto r = c1->use_thread_suspension_relaxed();
    assert(r);

    bool last_thread = c1 == &threads_[count_];
    finalize_thread_of_execution(last_thread);

    return r;
  });
  (void)c;
  // This point will be executed by the thread that finishes last.
}

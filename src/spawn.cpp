#include "concore2full/spawn.h"
#include "concore2full/c/bulk_spawn.h"
#include "concore2full/global_thread_pool.h"
#include "concore2full/profiling.h"

#include <chrono>
#include <cstring>

namespace {

using concore2full::detail::callcc;
using concore2full::detail::continuation_t;

/*
Valid transitions:
ss_initial_state -> ss_async_started --> ss_async_finished
                                     \-> ss_main_finishing -> ss_main_finished
*/
enum sync_state_values {
  ss_initial_state = 0,
  ss_async_started,
  ss_async_finished,
  ss_main_finishing,
  ss_main_finished,
};

//! Wait until the given function returns true.
template <typename F> void wait_with_backoff(F&& f) {
  constexpr int polling_count = 64;
  uint32_t i = 0;
  while (true) {
    if (f())
      return;
    // Do some polling.
    if (i++ < polling_count)
      continue;

    // Yield the control for this OS thread.
    std::this_thread::yield();
    i = 0;
  }
}

//! Wait until the given function, applied to the value read from `a`, returns true.
template <typename T, typename F> void atomic_wait(CONCORE2FULL_ATOMIC(T) & a, F&& f) {
  wait_with_backoff([&a, f = std::forward<F>(f)]() {
    return f(atomic_load_explicit(&a, std::memory_order_acquire));
  });
}

//! Called when the async work is finished, to see if we need a thread switch.
continuation_t on_async_complete(concore2full_spawn_frame* frame, continuation_t c) {
  int expected{ss_async_started};
  if (atomic_compare_exchange_strong(&frame->sync_state_, &expected, ss_async_finished)) {
    // We are first to arrive at completion.
    // We won't need any thread switch, so we can safely exit.
    // Return the original continuation.
    return c;
  } else {
    // We are the last to arrive at completion, and we need a thread switch.

    // If the main thread is currently finishing, wait for it to finish.
    // We need the main thread to properly call `originator_start`.
    atomic_wait(frame->sync_state_, [](int v) { return v == ss_main_finished; });

    // Finish the thread switch.
    return concore2full_exchange_thread_with(&frame->switch_data_.originator_);
  }
}

//! The task function that executes the async work.
void execute_spawn_task(concore2full_task* task, int) noexcept {
  auto frame = (concore2full_spawn_frame*)((char*)task - offsetof(concore2full_spawn_frame, task_));
  (void)callcc([frame](continuation_t thread_cont) -> continuation_t {
    // Assume there will be a thread switch and store required objects.
    concore2full_store_thread_data_relaxed(&frame->switch_data_.target_, thread_cont);
    // Signal the fact that we have started (and the continuation is properly stored).
    atomic_store_explicit(&frame->sync_state_, ss_async_started, std::memory_order_release);
    // Actually execute the given work.
    frame->user_function_(frame);
    // Complete the async processing.
    return on_async_complete(frame, thread_cont);
  });
}

} // namespace

extern "C" void concore2full_spawn(struct concore2full_spawn_frame* frame,
                                   concore2full_spawn_function_t f) {
  frame->task_.task_function_ = &execute_spawn_task;
  frame->task_.next_ = nullptr;
  frame->sync_state_ = ss_initial_state;
  frame->switch_data_.originator_.context_ = nullptr;
  frame->switch_data_.originator_.thread_reclaimer_ = nullptr;
  frame->switch_data_.target_.context_ = nullptr;
  frame->switch_data_.target_.thread_reclaimer_ = nullptr;
  frame->user_function_ = f;
  concore2full::global_thread_pool().enqueue(&frame->task_);
}

extern "C" void concore2full_await(struct concore2full_spawn_frame* frame) {
  // Ensure that we started the async work (and the continuation is set).
  atomic_wait(frame->sync_state_, [](int v) { return v >= ss_async_started; });
  // Now, the possible states are: ss_async_started, ss_async_finished

  int expected{ss_async_started};
  if (atomic_compare_exchange_strong(&frame->sync_state_, &expected, ss_main_finishing)) {
    // The main thread is first to finish; we need to start switching threads.
    auto c = callcc([frame](continuation_t await_cc) -> continuation_t {
      concore2full_store_thread_data_relaxed(&frame->switch_data_.originator_, await_cc);
      // We are done "finishing".
      atomic_store_explicit(&frame->sync_state_, ss_main_finished, std::memory_order_release);
      // Complete the thread switching.
      return concore2full_exchange_thread_with(&frame->switch_data_.target_);
    });
    (void)c;
  } else {
    // The async thread finished; we can continue directly, no need to switch threads.
  }
  // This point will be executed by the thread that finishes last.
}

extern "C" void concore2full_spawn2(struct concore2full_spawn_frame* frame,
                                    concore2full_spawn_function_t* f) {
  concore2full_spawn(frame, *f);
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

struct concore2full_bulk_spawn_task : concore2full_task {
  struct concore2full_bulk_spawn_frame* base_;
};

namespace {

int store_current_continuation(concore2full_bulk_spawn_frame* frame, continuation_t c) {
  // Occupy the next thread slot.
  int cont_index = atomic_fetch_add(&frame->started_tasks_, 1);
  assert(cont_index < frame->count_);
  // Store the thread data to the proper continuation index.
  // This is different from the index of the task, as we may store the continuation out of order.
  concore2full_store_thread_data_release(&frame->threads_[cont_index], c);
  return cont_index;
}

concore2full_thread_data* extract_continuation(concore2full_bulk_spawn_frame* frame) {
  // Obtain the index of the slot from which we need to extract.
  int index = atomic_fetch_add(&frame->completed_tasks_, 1);
  assert(index <= frame->count_);

  concore2full_thread_data* r = &frame->threads_[index];

  // Ensure that the thread data is properly initialized.
  // I.e., we didn't reach here before the other thread finished storing the data.
  atomic_wait(r->context_, [](auto c) { return c != nullptr; });

  return r;
}

void finalize_thread_of_execution(concore2full_bulk_spawn_frame* frame, bool is_last_thread) {
  // Obtain the index of the slot from which we need to extract.
  int count = frame->count_;
  atomic_fetch_add(&frame->finalized_tasks_, 1);
  if (is_last_thread) {
    // Last thread needs to ensure that all other threads have finalized their maintenance work
    // before returning to the continuation after the await point.
    atomic_wait(frame->finalized_tasks_, [count](int v) { return v == count + 1; });
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
    concore2full_thread_data* cont_data = extract_continuation(frame);
    if (cont_data == &frame->threads_[cont_index]) {
      // We are finishing on the same thread that started the task.
      finalize_thread_of_execution(frame, false);
      return thread_cont;
    } else {
      // We are finishing on a different thread than the one that started the task.
      auto r = concore2full_exchange_thread_with(cont_data);
      assert(r);

      bool last_thread = cont_data == &frame->threads_[frame->count_];
      finalize_thread_of_execution(frame, last_thread);

      return r;
    }
  });
}

} // namespace

extern "C" size_t concore2full_frame_size(int count) {
  return sizeof(concore2full_bulk_spawn_frame)            //
         + count * sizeof(concore2full_bulk_spawn_task)   //
         + (count + 1) * sizeof(concore2full_thread_data) //
      ;
}

void concore2full_bulk_spawn(struct concore2full_bulk_spawn_frame* frame, int count,
                             concore2full_bulk_spawn_function_t f) {
  size_t size_struct = sizeof(concore2full_bulk_spawn_frame);
  size_t size_tasks = count * sizeof(concore2full_bulk_spawn_task);
  char* p = reinterpret_cast<char*>(frame);
  frame->tasks_ = reinterpret_cast<concore2full_bulk_spawn_task*>(p + size_struct);
  frame->threads_ = reinterpret_cast<concore2full_thread_data*>(p + size_struct + size_tasks);

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
    frame->threads_[i].context_ = nullptr;
    frame->threads_[i].thread_reclaimer_ = nullptr;
  }

  concore2full::global_thread_pool().enqueue_bulk(frame->tasks_, count);
}
void concore2full_bulk_spawn2(struct concore2full_bulk_spawn_frame* frame, int count,
                              concore2full_bulk_spawn_function_t* f) {
  concore2full_bulk_spawn(frame, count, *f);
}

void concore2full_bulk_await(struct concore2full_bulk_spawn_frame* frame) {
  // If all the workers have finished, we can return directly.
  uint64_t completed = atomic_load_explicit(&frame->completed_tasks_, std::memory_order_acquire);
  if (completed == uint64_t(frame->count_))
    return;

  // We may need to switching threads, so we need a continuation.
  auto c = callcc([frame](continuation_t await_cc) -> continuation_t {
    // Store the current continuation, so that other threads can extract it.
    // We always store the continuation at `count_` position, so that this is the last one to be
    // extracted.
    concore2full::profiling::zone await_zone{CURRENT_LOCATION_N("await")};
    await_zone.set_param("ctx", (uint64_t)await_cc);
    concore2full_store_thread_data_release(&frame->threads_[frame->count_], await_cc);

    // Extract the next free continuation data and switch to it.
    auto c1 = extract_continuation(frame);
    auto r = concore2full_exchange_thread_with(c1);

    bool last_thread = c1 == &frame->threads_[frame->count_];
    finalize_thread_of_execution(frame, last_thread);

    return r;
  });
  (void)c;
  // This point will be executed by the thread that finishes last.
}

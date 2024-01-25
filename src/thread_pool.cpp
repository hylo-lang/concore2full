#include "concore2full/thread_pool.h"
#include "concore2full/profiling.h"
#include "concore2full/this_thread.h"
#include "concore2full/thread_reclaimer.h"
#include "concore2full/thread_snapshot.h"

#include <chrono>

using namespace std::chrono_literals;

namespace concore2full {

thread_pool::thread_pool() : thread_pool(std::thread::hardware_concurrency()) {}

thread_pool::thread_pool(int thread_count) : work_data_(thread_count) {
  threads_.reserve(thread_count);
  try {
    for (int i = 0; i < thread_count; i++) {
      threads_.emplace_back([this, i] { thread_main(i); });
    }
  } catch (...) {
    request_stop();
    join();
  }
}
thread_pool::~thread_pool() {
  request_stop();
  join();
}

void thread_pool::enqueue(concore2full_task* task) noexcept {
  task->next_ = nullptr;

  // Note: using uint32_t, as we need to safely wrap around.
  uint32_t thread_count = threads_.size();
  assert(thread_count > 0);
  uint32_t index = thread_index_to_push_to_.fetch_add(1, std::memory_order_relaxed) % thread_count;

  // Try to push this to a worker thread without blocking.
  for (uint32_t i = 0; i < thread_count; i++) {
    uint32_t current_index = (index + i) % thread_count;
    if (work_data_[current_index].try_push(task))
      return;
  }
  // If that didn't work, just force-push to the queue of the selected worker thread.
  uint32_t current_index = index % thread_count;
  work_data_[current_index].push(task);
}

void thread_pool::request_stop() noexcept {
  for (auto& d : work_data_) {
    d.request_stop();
  }
}

void thread_pool::join() noexcept {
  for (auto& t : threads_) {
    t.join();
  }
  threads_.clear();
}

void thread_pool::thread_data::request_stop() noexcept {
  std::lock_guard lock{bottleneck_};
  should_stop_ = true;
  cv_.notify_one();
}
bool thread_pool::thread_data::try_push(concore2full_task* task) noexcept {
  // Fail if we can't acquire the lock.
  std::unique_lock lock{bottleneck_, std::try_to_lock};
  if (!lock)
    return false;

  // Add the task at the back of the queue.
  bool was_empty = tasks_.empty();
  tasks_.push_back(task);
  // Wake up the worker thread if this is the only task in the queue.
  if (was_empty)
    cv_.notify_one();
  return true;
}
void thread_pool::thread_data::push(concore2full_task* task) noexcept {
  // Add the task at the back of the queue.
  std::lock_guard lock{bottleneck_};
  bool was_empty = tasks_.empty();
  tasks_.push_back(task);
  // Wake up the worker thread if this is the only task in the queue.
  if (was_empty)
    cv_.notify_one();
}
concore2full_task* thread_pool::thread_data::try_pop() noexcept {
  std::unique_lock lock{bottleneck_, std::try_to_lock};
  if (!lock || tasks_.empty())
    return nullptr;
  return tasks_.pop_front();
}
concore2full_task* thread_pool::thread_data::pop() noexcept {
  std::unique_lock lock{bottleneck_};
  while (tasks_.empty()) {
    if (should_stop_)
      return nullptr;
    this_thread::inversion_checkpoint();
    cv_.wait(lock);
  }
  return tasks_.pop_front();
}
void thread_pool::thread_data::wakeup() noexcept { cv_.notify_one(); }

void thread_pool::thread_main(int index) noexcept {
  profiling::zone zone{CURRENT_LOCATION()};
  zone.set_value(index);

  // Register a thread_reclaimer object
  struct my_thread_reclaimer : thread_reclaimer {
    thread_data* cur_thread_data_;
    explicit my_thread_reclaimer(thread_data* t) : cur_thread_data_(t) {}
    void start_reclaiming() override { cur_thread_data_->wakeup(); }
  };
  my_thread_reclaimer this_thread_reclaimer{&work_data_[index]};
  this_thread::set_thread_reclaimer(&this_thread_reclaimer);

  // We need to exit on the same thread.
  thread_snapshot t;

  int thread_count = threads_.size();
  while (true) {
    // First check if we need to restore this thread to somebody else.
    this_thread::inversion_checkpoint();

    concore2full_task* to_execute{nullptr};
    int current_index = 0;

    // Try to pop a task from the first thread data available.
    for (int i = 0; i < thread_count; i++) {
      current_index = (i + index) % thread_count;
      to_execute = work_data_[current_index].try_pop();
      if (to_execute)
        break;
    }

    // If we can't find anything available, block on our task queue.
    if (!to_execute) {
      current_index = index;
      to_execute = work_data_[current_index].pop();

      // If stop was requested, exit thread worker function.
      if (!to_execute)
        break;
    }

    assert(to_execute);
    profiling::zone zone2{CURRENT_LOCATION_N("execute")};
    to_execute->task_function_(to_execute, current_index);
  }

  // Ensure we finish on the same thread
  t.revert();
}

} // namespace concore2full
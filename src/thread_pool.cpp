#include "concore2full/thread_pool.h"
#include "concore2full/detail/sleep_helper.h"
#include "concore2full/profiling.h"
#include "concore2full/this_thread.h"
#include "concore2full/thread_reclaimer.h"
#include "concore2full/thread_snapshot.h"

#include <chrono>

using namespace std::chrono_literals;

namespace concore2full {

#ifndef NDEBUG
namespace {
//! Checks that the list represented by `head` is consistent.
bool check_list(concore2full_task* head, void* data) {
  concore2full_task* cur = head;
  while (cur) {
    assert(cur->prev_link_);
    assert(*cur->prev_link_ == cur);
    assert(cur->worker_data_ == data);
    cur = cur->next_;
  }
  return true;
}
} // namespace
#endif

namespace {
//! Return the desired level of conurrency.
size_t concurrency() {
  // Check if we have a maximum concurrency set as environment variable.
  const char* env_var = std::getenv("CONCORE_MAX_CONCURRENCY");
  if (env_var) {
    return std::stoul(env_var);
  }

  // Otherwise, return the hardware concurrency.
  return std::thread::hardware_concurrency();
}
} // namespace

thread_pool::thread_pool() : thread_pool(concurrency()) {}

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
  profiling::zone zone{CURRENT_LOCATION()};
  zone.set_param("task", reinterpret_cast<uint64_t>(task));
  zone.add_flow(reinterpret_cast<uint64_t>(task));

  task->next_ = nullptr;
  task->prev_link_ = nullptr;

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

bool thread_pool::extract_task(concore2full_task* task) noexcept {
  profiling::zone zone{CURRENT_LOCATION()};
  zone.set_param("task", reinterpret_cast<uint64_t>(task));
  zone.add_flow_terminate(reinterpret_cast<uint64_t>(task));
  auto d = static_cast<thread_data*>(task->worker_data_);
  return d ? d->extract_task(task) : false;
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

void thread_pool::wakeup() noexcept {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  for (auto& d : work_data_) {
    d.wakeup();
  }
}

void thread_pool::thread_data::request_stop() noexcept {
  std::unique_lock lock{bottleneck_};
  should_stop_ = true;
  wakeup_token_.notify();
}
bool thread_pool::thread_data::try_push(concore2full_task* task) noexcept {
  // Fail if we can't acquire the lock.
  std::unique_lock lock{bottleneck_, std::try_to_lock};
  if (!lock)
    return false;

  push_unprotected(task);
  return true;
}
void thread_pool::thread_data::push(concore2full_task* task) noexcept {
  // Add the task at the back of the queue.
  std::unique_lock lock{bottleneck_};
  push_unprotected(task);
}
concore2full_task* thread_pool::thread_data::try_pop() noexcept {
  std::unique_lock lock{bottleneck_, std::try_to_lock};
  if (!lock || !tasks_stack_)
    return nullptr;
  return pop_unprotected();
}
concore2full_task* thread_pool::thread_data::pop() noexcept {
  // If we we have some task, try to execute it.
  {
    std::unique_lock lock{bottleneck_};
    if (tasks_stack_)
      return pop_unprotected();
  }

  // If we are here, we need to wait for a task to be available.

  while (true) {
    // Start the sleeping process. After this, any notification will wake the thread up.
    detail::sleep_helper sleep_helper;
    // Extra check done just before sleeping.
    // These checks ensure that we always check important conditions before going to sleep.
    {
      std::unique_lock lock{bottleneck_};
      wakeup_token_.invalidate();

      // Check if we need to stop.
      if (should_stop_) {
        return nullptr;
      }

      // Check if we have new tasks.
      if (tasks_stack_) {
        return pop_unprotected();
      }
      // Store the wakeup token, so that we can wake up the thread if needed.
      wakeup_token_ = sleep_helper.get_wakeup_token();
    }
    // Now we can sleep.
    sleep_helper.sleep();
  }
}
bool thread_pool::thread_data::extract_task(concore2full_task* task) noexcept {
  profiling::zone zone{CURRENT_LOCATION()};
  zone.set_param("line", this);
  std::unique_lock lock{bottleneck_};
  assert(check_list(tasks_stack_, this));
  assert(!tasks_stack_ || tasks_stack_->prev_link_ == &tasks_stack_);
  if (task->worker_data_) {
    assert(task->worker_data_ == this);
    assert(task->prev_link_);
    assert(*task->prev_link_ == task);

    *task->prev_link_ = task->next_;
    if (task->next_)
      task->next_->prev_link_ = task->prev_link_;
    task->worker_data_ = nullptr;
    task->prev_link_ = nullptr;
    assert(tasks_stack_ != task);
    assert(!tasks_stack_ || tasks_stack_->prev_link_ == &tasks_stack_);
    assert(check_list(tasks_stack_, this));
    return true;
  } else {
    return false;
  }
}

void thread_pool::thread_data::wakeup() noexcept { wakeup_token_.notify(); }

void thread_pool::thread_data::push_unprotected(concore2full_task* task) noexcept {
  // Add the task in the front of the list.
  assert(check_list(tasks_stack_, this));
  bool was_empty = tasks_stack_ == nullptr;
  task->worker_data_ = this;
  task->next_ = tasks_stack_;
  if (tasks_stack_)
    tasks_stack_->prev_link_ = &task->next_;
  task->prev_link_ = &tasks_stack_;
  tasks_stack_ = task;
  assert(check_list(tasks_stack_, this));
  // Wake up the worker thread if this is the only task in the queue.
  if (was_empty)
    wakeup_token_.notify();
}

concore2full_task* thread_pool::thread_data::pop_unprotected() noexcept {
  assert(check_list(tasks_stack_, this));
  if (tasks_stack_) {
    concore2full_task* res = tasks_stack_;
    tasks_stack_ = tasks_stack_->next_;
    if (tasks_stack_)
      tasks_stack_->prev_link_ = &tasks_stack_;
    res->prev_link_ = nullptr;
    res->worker_data_ = nullptr;
    assert(check_list(tasks_stack_, this));
    return res;
  }
  return nullptr;
}

std::string thread_name(int index) { return "worker-" + std::to_string(index); }

void thread_pool::thread_main(int index) noexcept {
  concore2full::profiling::emit_thread_name_and_stack(thread_name(index).c_str());

  (void)profiling::zone_instant{CURRENT_LOCATION_N("worker thread start")};

  // Register a thread_reclaimer object
  struct my_thread_reclaimer : thread_reclaimer {
    // thread_data* cur_thread_data_;
    thread_pool* pool_;
    explicit my_thread_reclaimer(thread_pool* pool) : pool_(pool) {}
    void start_reclaiming() override {
      for (auto& d : pool_->work_data_) {
        d.wakeup();
      }
    }
  };
  my_thread_reclaimer this_thread_reclaimer{this};
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
    zone2.set_param("task", reinterpret_cast<uint64_t>(to_execute));
    zone2.add_flow_terminate(reinterpret_cast<uint64_t>(to_execute));
    to_execute->task_function_(to_execute, current_index);
  }

  // Ensure we finish on the same thread
  t.revert();

  (void)profiling::zone_instant{CURRENT_LOCATION_N("worker thread end")};
}

} // namespace concore2full
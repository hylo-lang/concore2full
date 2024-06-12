#include "concore2full/thread_pool.h"
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

thread_pool::thread_pool(int thread_count)
    : work_lines_(thread_count + 1) // +1 for extra threads
{
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
  uint32_t work_lines_count = work_lines_.size();
  assert(work_lines_count > 0);
  uint32_t index =
      thread_index_to_push_to_.fetch_add(1, std::memory_order_relaxed) % work_lines_count;

  // Try to push this to a worker thread without blocking.
  for (uint32_t i = 0; i < work_lines_count; i++) {
    uint32_t current_index = (index + i) % work_lines_count;
    if (work_lines_[current_index].try_push(task)) {
      return;
    }
  }
  // If that didn't work, just force-push to the queue of the selected worker thread.
  uint32_t current_index = index % work_lines_count;
  work_lines_[current_index].push(task);
}

bool thread_pool::extract_task(concore2full_task* task) noexcept {
  profiling::zone zone{CURRENT_LOCATION()};
  zone.set_param("task", reinterpret_cast<uint64_t>(task));
  zone.add_flow_terminate(reinterpret_cast<uint64_t>(task));
  auto d = static_cast<work_data*>(task->worker_data_);
  return d ? d->extract_task(task) : false;
}

void thread_pool::offer_help_until(std::stop_token stop_condition) noexcept {
  (void)profiling::zone{CURRENT_LOCATION()};

  // Register a stop callback that ensures that the thread is woken up.
  std::stop_callback callback(stop_condition, [this] { wakeup_all(); });
  // TODO: race condition when calling wakeup_all() when a thread is just preparing to go to sleep.

  // Run the loop to execute tasks.
  int index_hint = work_lines_.size() - 1;
  execute_work(stop_condition, index_hint);
}

void thread_pool::request_stop() noexcept {
  global_shutdown_.request_stop();
  wakeup_all();
}

void thread_pool::join() noexcept {
  for (auto& t : threads_) {
    t.join();
  }
  threads_.clear();
}

bool thread_pool::work_data::try_push(concore2full_task* task) noexcept {
  // Fail if we can't acquire the lock.
  std::unique_lock lock{bottleneck_, std::try_to_lock};
  if (!lock)
    return false;

  push_unprotected(task);
  return true;
}
void thread_pool::work_data::push(concore2full_task* task) noexcept {
  // Add the task at the back of the queue.
  std::lock_guard lock{bottleneck_};
  push_unprotected(task);
}
concore2full_task* thread_pool::work_data::try_pop() noexcept {
  std::unique_lock lock{bottleneck_, std::try_to_lock};
  if (!lock || !tasks_stack_)
    return nullptr;
  return pop_unprotected();
}
concore2full_task* thread_pool::work_data::pop(std::stop_token stop_condition) noexcept {
  std::unique_lock lock{bottleneck_};
  if (!tasks_stack_) {
    // Register a new thread_reclaimer object that allows us to wake up the thread if a thread
    // inversion is needed.
    struct my_thread_reclaimer : thread_reclaimer {
      std::condition_variable& cv_;
      explicit my_thread_reclaimer(std::condition_variable& cv) : cv_(cv) {}
      void start_reclaiming() override { cv_.notify_all(); }
    };
    my_thread_reclaimer this_thread_reclaimer{cv_};
    this_thread::set_thread_reclaimer(&this_thread_reclaimer);

    while (!tasks_stack_) {
      if (stop_condition.stop_requested())
        return nullptr;

      this_thread::inversion_checkpoint();
      num_waiting_threads_++;
      cv_.wait(lock);
      num_waiting_threads_--;
    }
  }
  return pop_unprotected();
}
bool thread_pool::work_data::extract_task(concore2full_task* task) noexcept {
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

void thread_pool::work_data::wakeup() noexcept {
  int num_waiting_threads = 0;
  {
    std::lock_guard lock{bottleneck_};
    num_waiting_threads = num_waiting_threads_;
  }
  for (int i = 0; i < num_waiting_threads; i++)
    cv_.notify_one();
}

void thread_pool::work_data::push_unprotected(concore2full_task* task) noexcept {
  // Add the task in the front of the list.
  assert(check_list(tasks_stack_, this));
  task->worker_data_ = this;
  task->next_ = tasks_stack_;
  if (tasks_stack_)
    tasks_stack_->prev_link_ = &task->next_;
  task->prev_link_ = &tasks_stack_;
  tasks_stack_ = task;
  assert(check_list(tasks_stack_, this));
  // If there are threads waiting on this work line, wake them up.
  if (num_waiting_threads_ > 0)
    cv_.notify_all();
}

concore2full_task* thread_pool::work_data::pop_unprotected() noexcept {
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

void thread_pool::wakeup_all() noexcept {
  for (auto& l : work_lines_) {
    l.wakeup();
  }
}

std::string thread_name(int index) { return "worker-" + std::to_string(index); }

void thread_pool::thread_main(int index) noexcept {
  concore2full::profiling::emit_thread_name_and_stack(thread_name(index).c_str());

  (void)profiling::zone_instant{CURRENT_LOCATION_N("worker thread start")};

  // We need to exit on the same thread.
  thread_snapshot t;

  // Run the loop to execute tasks.
  execute_work(global_shutdown_.get_token(), index);

  // Ensure we finish on the same thread
  t.revert();

  (void)profiling::zone_instant{CURRENT_LOCATION_N("worker thread end")};
}

void thread_pool::execute_work(std::stop_token stop_condition, int index_hint) noexcept {
  int work_lines_count = work_lines_.size();
  while (!stop_condition.stop_requested()) {
    // Acquire a work line to execute tasks from it, or to sleep on it.
    uint32_t work_line_index = index_hint % work_lines_count;
    work_data& our_work_line = work_lines_[work_line_index];

    // First check if we need to restore this thread to somebody else.
    this_thread::inversion_checkpoint();

    concore2full_task* to_execute{nullptr};
    int current_index = 0;

    // Try to pop a task from the first thread data available.
    // Note: we might pop a task from a different line
    for (int i = 0; i < work_lines_count * 2; i++) {
      current_index = (i + work_line_index) % work_lines_count;
      to_execute = work_lines_[current_index].try_pop();
      if (to_execute)
        break;
    }

    // If we can't find anything available, block on our task queue.
    if (!to_execute) {
      current_index = work_line_index;
      to_execute = our_work_line.pop(stop_condition);
    }

    // If stop was requested, exit the loop.
    if (!to_execute) {
      break;
    }

    assert(to_execute);
    profiling::zone zone2{CURRENT_LOCATION_N("execute")};
    zone2.set_param("task", reinterpret_cast<uint64_t>(to_execute));
    zone2.add_flow_terminate(reinterpret_cast<uint64_t>(to_execute));
    to_execute->task_function_(to_execute, current_index);
  }
}

} // namespace concore2full
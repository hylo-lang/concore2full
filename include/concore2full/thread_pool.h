#pragma once

#include "concore2full/c/task.h"
#include "concore2full/detail/catomic.h"
#include "concore2full/detail/sleep_helper.h"
#include "concore2full/profiling.h"

#include <cassert>
#include <mutex>
#include <thread>
#include <vector>

namespace concore2full {

/**
 * @brief A thread pool that can execute work.
 *
 * This will start a number of threads, each of them able to execute tasks. If not specified, the
 * number of threads will match the available concurrency on the target hardware; doing this will
 * try to ensure that we are properly utilize hardware resources to maximize throughput.
 */
class thread_pool {
public:
  //! Constructor. Using hardware available parallelism to size the pool of threads.
  thread_pool();
  //! Constructor. Using specified number of threads.
  explicit thread_pool(int num_threads);
  //! Destructor. Waits for all the threads to be done.
  ~thread_pool();

  /**
   * @brief Enqueue a task for execution.
   * @param task The task to be executed on this thread pool.
   */
  void enqueue(concore2full_task* task) noexcept;

  /**
   * @brief Bulk enqueue a number of tasks.
   * @param tasks Array of tasks that need to be executed.
   * @param count The number of tasks in the array.
   */
  template <std::derived_from<concore2full_task> Task>
  void enqueue_bulk(Task* tasks, int count) noexcept {
    for (int i = 0; i < count; i++) {
      enqueue(&tasks[i]);
    }
  }

  /**
   * @brief Extracts a task that was scheduled from execution.
   * @param task The task that should not be executed anymore.
   * @return `true` if the task was extracted; false if the task started executing or extracted.
   *
   * If the execution of `task` hasn't started yet, it will not be started anymore. If the task
   * started executing, this will return `false`.
   */
  bool extract_task(concore2full_task* task) noexcept;

  //! Requests the thread to stop. The threads will stop after executing all the submitted work.
  void request_stop() noexcept;
  //! Waits for all the threads to complete; should be called after `request_stop()`.
  void join() noexcept;

  //! Returns the number of threads in `this`.
  int available_parallelism() const noexcept { return threads_.size(); }

private:
  //! Data corresponding to a worker thread.
  class worker_thread_data {
  public:
    template <typename F>
    explicit worker_thread_data(F&& f, int work_line_start_index)
        : work_line_start_index_(work_line_start_index), thread_((std::forward<F>(f))) {}

    //! If sleeping, wakeup the threads and ask it to execute work on `work_line_hint` work line.
    //! Returns `true` if a thread is woken up.
    bool try_notify(int work_line_hint) noexcept;

    //! Attempts to put the thread to sleep, until the thread is notified or `stop_requested` is
    //! `true`.
    int sleep(std::atomic<bool>& stop_requested) noexcept;

    //! Called to end up the thread.
    void join();

  private:
    //! Token used to wake up the thread.
    detail::wakeup_token wakeup_token_;
    //! The work line index to start working from.
    detail::catomic<int> work_line_start_index_;
    //! The number of notifies that are pending.
    //! Zero means that the thread is sleeping; a value greater than zero, it means that the thread
    //! is awake (or waking up).
    detail::catomic<int> wake_requests_{1};
    //! The thread that is executing the work.
    std::thread thread_;
  };

  //! Collection of tasks that need to be executed.
  //! Instead of placing all tasks into a single collection, we use multiple such objects to reduce
  //! contention.
  class work_line {
  public:
    /**
     * @brief Try pushing a task into the list of tasks.
     * @param task The task that needs to be executed.
     * @return True if succeeded to adding the task.
     *
     * If there is another thread that has the acquired the lock (for pushing or popping tasks),
     * this operation will fail. We do this to avoid contention and putting the thread to sleep.
     *
     * @sa push()
     */
    bool try_push(concore2full_task* task) noexcept;

    /**
     * @brief Pushes a task to the list of tasks.
     * @param task The task that needs to be executed.
     *
     * If the mutex is already taken, this will block waiting for the mutex to be unblocked.
     *
     * @sa try_push()
     */
    void push(concore2full_task* task) noexcept;

    /**
     * @brief Try popping a task to execute.
     * @return The task that needs to be executed, or null.
     *
     * If there are no tasks in the list, or if the mutex around the list is taken, this will
     * return nullptr. By not blocking to wait for the result, we are trying to avoid putting the
     * thread to sleep while there are tasks on other threads that can be executed.
     *
     * @sa pop()
     */
    [[nodiscard]] concore2full_task* try_pop() noexcept;

    //! Removes `task` from the list of tasks.
    bool extract_task(concore2full_task* task) noexcept;

  private:
    //! Mutex used to protect the access to the task list.
    std::mutex bottleneck_;
    //! The stack of tasks that need to be executed.
    concore2full_task* tasks_stack_{nullptr};

    //! Pushes `task` to the worker, without worrying about the lock.
    void push_unprotected(concore2full_task* task) noexcept;

    //! Pops a task from the worker, without worrying about the lock.
    [[nodiscard]] concore2full_task* pop_unprotected() noexcept;
  };

  //! Data corresponding to each working thread, containing the list of tasks that need to be
  //! executed.
  std::vector<work_line> work_lines_;
  //! The number of tasks that are currently in the thread pool.
  std::atomic<int> num_tasks_;

  //! The index of the next line to get new tasks. We use unsigned integers as we want this value
  //! to nicely wrap around. The value can be bigger than the actual number of work lines.
  std::atomic<uint32_t> line_to_push_to_{0};

  //! True if we are requested to stop any work on the thread_pool.
  std::atomic<bool> stop_requested_{false};

  //! The threads that are doing the work.
  std::vector<worker_thread_data> threads_;

  void notify_one(int work_line_hint) noexcept;

  /**
   * @brief The main function to be executed by the worker threads
   * @param index The index of the current thread.
   *
   * This will try to execute as much as possible tasks. First, it tries to get tasks from the list
   * associated with the current thread. If there are no tasks there, or if there is contention on
   * that list, it will try to take tasks from other lists. If there are no tasks to execute, this
   * will wait for tasks to appear in the list corresponding to the current thread.
   *
   * @sa work_line
   */
  void thread_main(int index) noexcept;
};

} // namespace concore2full
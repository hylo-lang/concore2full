#pragma once

#include "concore2full/c/task.h"
#include "concore2full/profiling.h"

#include <cassert>
#include <condition_variable>
#include <mutex>
#include <semaphore>
#include <stop_token>
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

  //! Makes the current thread join the thread pool until `stop_condition` is set, executing work
  //! from the pool.
  void offer_help_until(std::stop_token stop_condition) noexcept;

  //! Requests the thread to stop. The threads will stop after executing all the submitted work.
  void request_stop() noexcept;
  //! Waits for all the threads to complete; should be called after `request_stop()`.
  void join() noexcept;

  //! Returns the number of threads in `this`.
  int available_parallelism() const noexcept { return threads_.size(); }

private:
  //! Keeps track of a subset of tasks that need to be executed.
  //! The pool has multiple instances of this class.
  //! Typically, we try to associate one of this object to a thread.
  class work_data {
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

    /**
     * @brief Pop a task to execute
     * @param stop_condition Indicates when we should stop executing tasks or sleeping on this line.
     * @return The task to be executed, or null if stop was requested
     *
     * This will attempt to get a task from the list to be executed. If the mutex is already
     * acquired by some other thread, this will block until the mutex is released. If there is not
     * task to be executed, this will block until there is one to execute.
     *
     * If stop was requested, and there is no task in the list, this will return `nullptr` without
     * blocking.
     *
     * @sa try_pop()
     */
    [[nodiscard]] concore2full_task* pop(std::stop_token stop_condition) noexcept;

    //! Removes `task` from the list of tasks.
    bool extract_task(concore2full_task* task) noexcept;

    //! If a thread is waiting on `this`, wake it up.
    //! Useful when we need to check stop conditions or when the current thread needs to be
    //! reclaimed.
    void wakeup() noexcept;

  private:
    //! The stack of tasks that need to be executed.
    concore2full_task* tasks_stack_{nullptr};
    //! The number of threads that are waiting on this work line.
    int num_waiting_threads_{0};
    //! Mutex used to protect the access to `tasks_stack_` and `num_waiting_threads_`.
    std::mutex bottleneck_;
    //! Conditional variable used to wait on when there is no task to be executed.
    std::condition_variable cv_;

    //! Pushes `task` to the worker, without worrying about the lock.
    void push_unprotected(concore2full_task* task) noexcept;

    //! Pops a task from the worker, without worrying about the lock.
    [[nodiscard]] concore2full_task* pop_unprotected() noexcept;
  };

  //! The threads that are doing the work.
  std::vector<std::thread> threads_;
  //! The tasks that need to be executed, distributed among multiple "lines", so that they can be
  //! handled in parallel by different threads
  //! We may have more work lines than actual threads.
  std::vector<work_data> work_lines_;
  //! The index of the next thread to get new tasks. We use unsigned integers as we want this value
  //! to nicely wrap around. The value can be bigger than the actual number of threads.
  std::atomic<uint32_t> thread_index_to_push_to_{0};

  //! The global stop source that can be used to stop all the threads.
  std::stop_source global_shutdown_;

  //! Wake up all the threads sleeping on work lines.
  void wakeup_all() noexcept;

  /**
   * @brief The main function to be executed by the worker threads
   * @param index The index of the current thread.
   *
   * This will try to execute as much as possible tasks. First, it tries to get tasks from the list
   * associated with the current thread. If there are no tasks there, or if there is contention on
   * that list, it will try to take tasks from other lists. If there are no tasks to execute, this
   * will wait for tasks to appear in the list corresponding to the current thread.
   *
   * @sa work_data
   */
  void thread_main(int index) noexcept;

  //! Execute work from the thread pool until `stop_condition` is set.
  //! Tries to use the work line with index `index_hint` first, but may use other lines, and can
  //! steal tasks from other threads.
  void execute_work(std::stop_token stop_condition, int index_hint) noexcept;
};

} // namespace concore2full
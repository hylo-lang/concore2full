#pragma once

#include "concore2full/detail/task.h"
#include "concore2full/detail/task_queue.h"
#include "concore2full/profiling.h"

#include <condition_variable>
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
  thread_pool(int num_threads);
  //! Destructor. Waits for all the threads to be done.
  ~thread_pool();

  /**
   * @brief Enqueue a task for execution.
   * @param task The task to be executed on this thread pool.
   */
  void enqueue(detail::task_base* task) noexcept;

  //! Requests the thread to stop. The threads will stop after executing all the submitted work.
  void request_stop() noexcept;
  //! Waits for all the threads to complete; should be called after `request_stop()`.
  void join() noexcept;

  //! Returns the number of threads in `this`.
  int available_parallelism() const noexcept { return threads_.size(); }

private:
  //! Data corresponding to a thread. Contains the queue of tasks corresponding to this thread, and
  //! the required syncrhonization.
  class thread_data {
  public:
    //! Requests the thread operating on this data to stop.
    void request_stop() noexcept;

    /**
     * @brief Try pusing a task into the queue.
     * @param task The task that neds to be executed.
     * @return True if succeeded to enqueue the task.
     *
     * If there is another thread that has the acquired the lock (for pushing or popping tasks),
     * this operation will fail. We do this to avoid contention and putting the thread to sleep.
     *
     * @sa push()
     */
    bool try_push(detail::task_base* task) noexcept;

    /**
     * @brief Pushes a task to the queue.
     * @param task The task that needs to be executed.
     *
     * If the mutex is already taken, this will block waiting for the mutex to be unblocked.
     *
     * @sa try_push()
     */
    void push(detail::task_base* task) noexcept;

    /**
     * @brief Try popping a task to execute.
     * @return The task that needs to be executed, or null.
     *
     * If there are no tasks in the queue, or if the mutex around the queue is taken, this will
     * return nullptr. By not blocking to wait for the result, we are trying to avoid putting the
     * thread to sleep while there are tasks on other threads that can be executed.
     *
     * @sa pop()
     */
    [[nodiscard]] detail::task_base* try_pop() noexcept;

    /**
     * @brief Pop a task to execute
     * @return The task to be executed, or null if stop was requested
     *
     * This will attempt to get a task from the queue to be executed. If the mutex is already
     * acquired by some other thread, this will block until the mutex is released. If there is not
     * task to be executed, this will block until there is one to execute.
     *
     * If stop was requested, and there is no task in the queue, this will return `nullptr` without
     * blocking.
     *
     * @sa try_pop()
     */
    [[nodiscard]] detail::task_base* pop() noexcept;

  private:
    //! Mutex used to protect the access to the task queue.
    std::mutex bottleneck_;
    //! Conditional variable used to wait on when there is no task to be executed.
    std::condition_variable cv_;
    //! The queue of tasks to be executed.
    detail::task_queue tasks_;
    //! Indicates when the we should not block while waiting for new task, ending the current worker
    //! thread.
    bool should_stop_{false};
  };

  //! The threads that are doing the work.
  std::vector<std::thread> threads_;
  //! Data corresponding to each working thread, containing the queue of tasks that need to be
  //! executed.
  std::vector<thread_data> work_data_;
  //! The index of the next thread to get new tasks. We use unsigned integers as we want this value
  //! to nicely wrap around. The value can be bigger than the actual number of threads.
  std::atomic<uint32_t> thread_index_to_push_to_{0};

  /**
   * @brief The main function to be executed by the worker threads
   * @param index The index of the current thread.
   *
   * This will try to execute as much as possible tasks. First, it tries to get tasks from the queue
   * associated with the current thread. If there are no tasks there, or if there is contention on
   * that queue, it will try to take tasks from other queues. If there are no tasks to execute, this
   * will wait for tasks to appear in the queue corresponding to the current thread.
   *
   * @sa thread_data
   */
  void thread_main(int index) noexcept;
};

} // namespace concore2full
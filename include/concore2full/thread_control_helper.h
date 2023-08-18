#pragma once

#include <atomic>

namespace concore2full {

/**
 * @brief Defines the entitiy that needs to be notified when reclaiming a thread.
 *
 * After a thread inversion, any thread can reach into a thread pool. If we want to reclaim the
 * thread (i.e., reverse the thread inversion), we need to be able to tell the thread pool that it
 * needs to let the thread go. This interface allows to send that message to the thread pool.
 *
 * A thread can be executing a series of tasks, or it can be sleeping while waiting for tasks. In
 * both cases, we need to be able to tell the internal state machine of the thread pool to check the
 * reclamation of the thread.
 *
 * Thread pools need to create such an object for each thread, and register it through:
 *  thread_control_helper::set_current_thread_reclaimer()
 *
 * After `start_reclaiming()` is called, the corresponding thread is expected to call
 * `thread_control_helper::check_for_thread_inversion()`.
 *
 * @sa thread_control_helper
 */
class thread_reclaimer {
public:
  virtual ~thread_reclaimer() = default;
  //! Tell the owner of the thread that we want to reclaim the thread.
  virtual void start_reclaiming() = 0;
};

/**
 * @brief Helper class to manage the ownership of threads.
 *
 * When spawning tasks, we might have "thread inversion": the threads are swapping execution paths
 * at the end of the computation (the `await` point). While this is fine for most of concurrent
 * processing, there are times in which we need to guarantee that a computation ends on the same
 * thread that it starts (this is especially true if external systems are calling to us).
 *
 * If we need to ensure that the computation ends on the same thread, and we had at least one thread
 * inversion, we need to have antother thread inversion to get the original thread back. This class
 * provides the low-level machinery to implement this.
 *
 * We allow to associate with each thread a `thread_reclaimer`. This object is called to notify the
 * current owner of the thread that it needs to let the thread be inverted with some other thread.
 *
 * To ensure we keep the original thread, one can use this calls in the following way:
 *  - creates an instance of this type on the original thread
 *  - once the computation is done (which may continue on a different thread), we call
 * `ensure_starting_thread()` on this object. This will initiate a thread inversion if the current
 * thread is different than the original thread.
 *  - if a thread switch is requested, the current owner of the thread will be notified to let the
 * thread participate in the thread inversion.
 *
 * @sa thread_reclaimer, sync_execute()
 */
class thread_control_helper {
public:
  //! Constructor. Gets information about the currently working thread.
  thread_control_helper();
  //! Destructor.
  ~thread_control_helper();

  //! Get the thread_reclaimer object associated with the current thread.
  static thread_reclaimer* get_current_thread_reclaimer();

  //! Sets the thread_reclaimer object for the current thread.
  static void set_current_thread_reclaimer(thread_reclaimer* new_reclaimer);

  //! Performs a thread inversion if needed, to resume execution on the same thread that the
  //! constructor was called. Needs to be called before the destructor if the current thread is
  //! different.
  void ensure_starting_thread();

  //! Called by the original thread when it is notified that it needs to be reclaimed.
  static void check_for_thread_inversion();

private:
  struct switch_data;

  //! Data for thread inversion.
  //! When this is present, the thread that need to be reclaimed will perform the inversion.
  //! Stored on the thread that is requesting the inversion.
  std::atomic<switch_data*> should_switch_{nullptr};

  //! The previous control helper on this thread. Used to deal with cases of nested `sync_execute`
  //! calls.
  thread_control_helper* previous_{nullptr};

  //! The address of the thread_reclaimer object for the original thread.
  thread_reclaimer** reclaimer_addrress_{nullptr};
};

} // namespace concore2full

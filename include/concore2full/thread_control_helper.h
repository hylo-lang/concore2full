#pragma once

#include <atomic>

namespace concore2full {

struct thread_info;
class thread_reclaimer;

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
  //! Get the thread_reclaimer object associated with the current thread.
  static thread_reclaimer* get_current_thread_reclaimer();

  //! Sets the thread_reclaimer object for the current thread.
  static void set_current_thread_reclaimer(thread_reclaimer* new_reclaimer);

  //! Called by the original thread when it is notified that it needs to be reclaimed.
  static void check_for_thread_inversion();
};

/**
 * @brief A snapshot of the currrent thread.
 *
 * Keeps track of the current thread. We have the ability to get back to the thread that was used
 * when the constructor was called.
 */
class thread_snapshot {
public:
  //! Constructor. Gets information about the currently working thread.
  thread_snapshot();
  //! Destructor.
  ~thread_snapshot() = default;

  //! Get back to the thread on which the constructor was called.
  void revert();

private:
  //! The original thread.
  thread_info* original_thread_{nullptr};

  /**
   * @brief Wait until we can start the switch of control flows
   * @return `true` if we still need to perform the switch.
   */
  bool wait_for_switch_start();

  //! Perform the control flow switch to get back to the original thread.
  void perform_switch();
};

} // namespace concore2full

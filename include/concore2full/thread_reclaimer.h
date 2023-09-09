#pragma once

namespace concore2full {

/**
 * @brief Defines the entitiy that needs to be notified when reclaiming a thread.
 *
 * To be used by library authors who design thread pool classes.
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

} // namespace concore2full

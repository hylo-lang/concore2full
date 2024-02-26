#pragma once

namespace concore2full {

namespace detail {
struct thread_info;
}

/**
 * @brief A snapshot of the current thread.
 *
 * Keeps track of the current thread. We have the ability to get back to the thread that was used
 * when the constructor was called.
 *
 * Prefer using `sync_execute()` as a wrapper on top of this abstraction.
 *
 * @sa sync_execute()
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
  detail::thread_info* original_thread_{nullptr};

  /**
   * @brief Wait until we can start the switch of control flows
   * @return `true` if we still need to perform the switch.
   */
  bool wait_for_switch_start();

  //! Perform the control flow switch to get back to the original thread.
  void perform_switch();
};

} // namespace concore2full
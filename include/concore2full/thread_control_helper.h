#pragma once

#include <concore2full/detail/callcc.h>

#include <concepts>
#include <condition_variable>
#include <mutex>
#include <semaphore>

namespace concore2full {

// TODO: wake up thread

class thread_control_helper {
public:
  thread_control_helper();
  ~thread_control_helper();

  void ensure_starting_thread();

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
};

} // namespace concore2full
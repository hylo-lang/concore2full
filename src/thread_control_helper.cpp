#include <concore2full/thread_control_helper.h>

#include <concore2full/detail/callcc.h>
#include <concore2full/detail/thread_switch_helper.h>

#include <mutex>
#include <semaphore>
#include <utility>

namespace concore2full {

//! Structure holding global data needed for thread control.
struct thread_control_data {
  std::mutex dependency_bottleneck_;
};

//! A single instance of `thread_control_data`.
static thread_control_data g_thread_control_data;

struct thread_info;

//! Data used for switching control flows between threads.
struct thread_switch_control {
  //! True if this thread started to switch control flows with another thread.
  //! Will be set only for the originating thread.
  bool is_currently_switching_{false};
  //! The thread that the originator thread is switching control flow with.
  thread_info* waiting_on_thread_{nullptr};

  //! Semaphore used to wait for the other thread to enter switching mode.
  std::binary_semaphore waiting_semaphore_{0};
  //! Indicates if this thread should join the switch process initiated by the value stored in here.
  std::atomic<thread_info*> should_switch_with_{nullptr};

  /**
   * @brief Mark the beginning of switching the current control flow from `self` thread to `t`.
   * @param self  The current thread, from which we initiate the control flow switch
   * @param t     The desired thread on which the originating control flow needs to switch to.
   * @return `true` if the thread switch is started; `false` if we cannot start the switch at this
   * point.
   */
  bool request_switch_to(thread_info* self, thread_info* t);
  //! Mark that the control flow switch was complete.
  void switch_complete();
};

//! Describes the data associated for each thread, used for controlling the swiching of control
//! flows.
struct thread_info {
  //! The object that needs to be notified when we want to switch control flow with another thread.
  thread_reclaimer* thread_reclaimer_{nullptr};
  //! The data used for swtiching control flow with other threads.
  thread_switch_control switch_control_;
  //! Data used to switch threads between control-flows.
  detail::thread_switch_helper switch_data_;
};

//! The data associated with each thread.
thread_local thread_info tls_thread_info_;

bool thread_switch_control::request_switch_to(thread_info* self, thread_info* t) {
  assert(self != t);
  std::lock_guard<std::mutex> lock{g_thread_control_data.dependency_bottleneck_};
  if (!t->switch_control_.is_currently_switching_) {
    // Mark the start of the switch.
    is_currently_switching_ = true;
    waiting_on_thread_ = t;
    return true;
  } else {
    return false;
  }
}
void thread_switch_control::switch_complete() {
  std::lock_guard<std::mutex> lock{g_thread_control_data.dependency_bottleneck_};
  is_currently_switching_ = false;
  waiting_on_thread_->switch_control_.should_switch_with_.store(nullptr, std::memory_order_relaxed);
  waiting_on_thread_ = nullptr;
}

thread_reclaimer* thread_control_helper::get_current_thread_reclaimer() {
  return tls_thread_info_.thread_reclaimer_;
}

void thread_control_helper::set_current_thread_reclaimer(thread_reclaimer* new_reclaimer) {
  tls_thread_info_.thread_reclaimer_ = new_reclaimer;
}

void thread_control_helper::check_for_thread_inversion() {
  // Check if some other thread requested us to switch.
  auto* first_thread =
      tls_thread_info_.switch_control_.should_switch_with_.load(std::memory_order_acquire);
  if (first_thread) {
    // The switch data will be stored on the first thread.
    (void)detail::callcc([first_thread](detail::continuation_t c) -> detail::continuation_t {
      first_thread->switch_data_.secondary_start(c);
      auto next_for_us = first_thread->switch_data_.secondary_end();

      first_thread->switch_control_.waiting_semaphore_.release();

      return next_for_us;
    });
    // The originating thread will continue this control flow.
  }
}

thread_snapshot::thread_snapshot() { original_thread_ = &tls_thread_info_; }

void thread_snapshot::revert() {
  // Are we on the same thread?
  thread_info* cur_thread = &tls_thread_info_;
  if (cur_thread == original_thread_) {
    // Good. No waiting needs to happen.
  } else {
    // Wait until we can start the switch.
    // By the time waiting is over, we may be back on the desired thread, so don't do the switch in
    // that case.
    if (wait_for_switch_start())
      perform_switch();
  }
}

bool thread_snapshot::wait_for_switch_start() {
  while (true) {
    auto* cur_thread = &tls_thread_info_;
    if (cur_thread->switch_control_.request_switch_to(cur_thread, original_thread_)) {
      // We started the switch process.
      return true;
    } else {
      // We cannot switch at this moment.
      // Check if another thread requested a switch from us.
      thread_control_helper::check_for_thread_inversion();
      // Now, this function may return on a different thread; check if we still need to switch.
      if (&tls_thread_info_ == original_thread_) {
        return false;
      }
      // If we need to switch, introduce a small pause, so that we don't consume the CPU while
      // spinning.
      std::this_thread::yield();
    }
  }
}

void thread_snapshot::perform_switch() {
  // Request a thread inversion. We might need to wait if our original thread is currently busy.
  (void)detail::callcc([this](detail::continuation_t c) -> detail::continuation_t {
    // Use the switch data from our thread.
    auto* switch_data = &tls_thread_info_.switch_data_;
    switch_data->originator_start(c);
    // TODO: fix race condition here; multiple threads may be trying to store at the same time
    original_thread_->switch_control_.should_switch_with_.store(&tls_thread_info_,
                                                                std::memory_order_release);
    // Note: After this line, the other thread can anytime switch to the `after_originator_`
    // continuation, destryoing `this` pointer.

    // If this thread is controlled by a thread pool which has a reclaimer registered, tell it to
    // start reclaiming.
    auto* reclaimer = original_thread_->thread_reclaimer_;
    if (reclaimer)
      reclaimer->start_reclaiming();

    // Block until the original thread can perform the thread switch
    tls_thread_info_.switch_control_.waiting_semaphore_.acquire();

    // The thread switch is complete.
    // TODO: check for race conditions
    tls_thread_info_.switch_control_.switch_complete();

    // Switch to the continuation provided by our original thread.
    return tls_thread_info_.switch_data_.originator_end();
  });
  // We resume here on the original thread.
  assert(original_thread_ == &tls_thread_info_);
}

} // namespace concore2full
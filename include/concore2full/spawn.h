#pragma once

#include "concore2full/detail/callcc.h"
#include "concore2full/detail/task_base.h"
#include "concore2full/detail/thread_switch_helper.h"
#include "concore2full/global_thread_pool.h"
#include "concore2full/this_thread.h"

namespace concore2full {

/// @brief The state of a spawned execution.
///
/// @tparam Fn The type of the functor used for spawning concurrent work.
///
/// This represents the result of a `spawn` call, containing the state associated with the spawned
/// work. It will hold the data needed to execute the spawned computation and get its results.
///
/// It can only be constructed through calling the `spawn()` function.
///
/// It provides mechanisms for the main thread of execution to *await* the finish of the
/// computation. That is, continue when the computation is finished. If the main thread of execution
/// finishes first, then *thread inversion* will happen on the await point: the spawned thread will
/// continue the main work, while the main thread will be suspended. However, no OS thread will be
/// blocked.
///
/// It exposes the type of the result of the spanwed computation.
///
/// If the object is destryoed before the spawned work finishes, the work will be cancelled.
///
/// @sa spawn()
template <typename Fn> class spawn_state : private detail::task_base {
public:
  ~spawn_state() = default;
  // No copy, no move
  spawn_state(const spawn_state&) = delete;

  /// The type returned by the spawned computation.
  using res_t = std::invoke_result_t<Fn>;

  /// @brief Await the result of the computation
  ///
  /// @return The result of the computation; throws if the operation was cancelled.
  ///
  /// If the main thread of execution arrives at this point after the work is done, this will simply
  /// return the result of the work. If the main thread of exection arrives here before the work is
  /// completed, a "thread inversion" happens, and the main thread of execution continues work on
  /// the coroutine that was just spawned.
  res_t await() {
    on_main_complete();
    return res_;
  }

private:
  enum class sync_state {
    both_working,
    main_finishing,
    main_finished,
    async_finished,
  };

  /// The function that needs to be executed in the spawned work.
  Fn f_;
  /// The location where we need to store the result
  res_t res_;
  /// The state of the computation, with respect to reaching the await point.
  std::atomic<sync_state> sync_state_{sync_state::both_working};
  //! Indicates that the async processing has started (continuation is set).
  std::atomic<bool> async_started_{false};
  //! Data used to switch threads between control-flows.
  detail::thread_switch_helper switch_data_;
  /// A snaphot of the profilng zones at that spawn point.
  profiling::zone_stack_snapshot zones_;

  /// @brief  Called when the async work is completed
  /// @return The continuation that the work coroutine needs to do next (if there is any).
  detail::continuation_t on_async_complete(detail::continuation_t c) {
    sync_state expected{sync_state::both_working};
    if (sync_state_.compare_exchange_strong(expected, sync_state::async_finished)) {
      // We are first to arrive at completion.
      // We won't need any thread switch, so we can safely exit.
      // Return the original continuation.
      return c;
    } else {
      // We are the last to arrive at completion, and we need a thread switch.

      // If the main thread is currently finishing, wait for it to finish.
      // We need the main thread to properly call `originator_start`.
      sync_state_.wait(sync_state::main_finishing, std::memory_order_acquire);

      // Finish the thread switch.
      return switch_data_.secondary_end();
    }
  }

  /// Called when the main thread of execution completes.
  void on_main_complete() {
    sync_state expected{sync_state::both_working};
    if (sync_state_.compare_exchange_strong(expected, sync_state::main_finishing)) {
      // The main thread is first to finish; we need to start switching threads.
      auto c = detail::callcc([this](detail::continuation_t await_cc) -> detail::continuation_t {
        this->switch_data_.originator_start(await_cc);
        // We are done "finishing".
        sync_state_.store(sync_state::main_finished, std::memory_order_release);
        sync_state_.notify_one();
        // Ensure that we started the async work (and the continuation is set).
        async_started_.wait(false, std::memory_order_acquire);
        // Complete the thread switching.
        return this->switch_data_.originator_end();
      });
      (void)c;
    } else {
      // The async thread finished; we can continue directly, no need to switch threads.
    }
    // This point will be executed by the thread that finishes last.
  }

  void execute(int) noexcept {
    profiling::duplicate_zones_stack scoped_zones_stack{zones_};
    (void)detail::callcc([this](detail::continuation_t thread_cont) -> detail::continuation_t {
      // Assume there will be a thread switch and store required objects.
      switch_data_.secondary_start(thread_cont);
      // Signal the fact that we have started (and the continuation is properly stored).
      async_started_.store(true, std::memory_order_release);
      async_started_.notify_one();
      // Actually execute the given work.
      res_ = std::invoke(std::forward<Fn>(f_));
      // Complete the async processing.
      return on_async_complete(thread_cont);
    });
  }

  template <typename F> friend auto spawn(F&& f);

  spawn_state(Fn&& f) : f_(std::forward<Fn>(f)) { global_thread_pool().enqueue(this); }
};

/// @brief  Spawn work with the default scheduler
/// @tparam Fn The type of the function to execute
/// @param f The function representing the work that needs to be executed concurrently.
///
/// This will use the default scheduler to spawn new work concurrently.
template <typename Fn> inline auto spawn(Fn&& f) {
  using op_t = spawn_state<Fn>;
  return op_t{std::forward<Fn>(f)};
}

} // namespace concore2full
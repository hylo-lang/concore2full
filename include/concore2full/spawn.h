#pragma once

#include "concore2full/detail/callcc.h"
#include "concore2full/detail/task_base.h"
#include "concore2full/global_thread_pool.h"

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
    first_finishing,
    first_finished,
    second_finished,
  };

  /// The function that needs to be executed in the spawned work.
  Fn f_;
  /// The location where we need to store the result
  res_t res_;
  /// TODO: check if we can remove this.
  detail::continuation_t cont_;
  /// The state of the computation, with respect to reaching the await point.
  std::atomic<sync_state> sync_state_{sync_state::both_working};
  /// Continuation pointing to the code after the `await`, on the main thread of execution.
  detail::continuation_t main_cont_;
  /// Continuation pointing to the end of the thread.
  detail::continuation_t thread_cont_;
  /// A snaphot of the profilng zones at that spawn point.
  profiling::zone_stack_snapshot zones_;

  /// @brief  Called when the async work is completed
  /// @return The continuation that the work coroutine needs to do next (if there is any).
  detail::continuation_t on_async_complete() {
    sync_state expected{sync_state::both_working};
    if (sync_state_.compare_exchange_strong(expected, sync_state::first_finished)) {
      // We are first to arrive at completion.
      // There is nothing for this thread to do here, we can safely exit.
      return nullptr;
    } else {
      // If the main thread is currently finishing, wait for it to finish.
      while (sync_state_.load() != sync_state::first_finished)
        ; // wait
      // TODO: exponential backoff

      // We are the last to arrive at completion.
      // The main thread set the continuation point; we need to jump there.
      return std::exchange(main_cont_, nullptr);
    }
  }

  /// Called when the main thread of execution completes.
  void on_main_complete() {
    auto c = detail::callcc([this](detail::continuation_t await_cc) -> detail::continuation_t {
      sync_state expected{sync_state::both_working};
      if (sync_state_.compare_exchange_strong(expected, sync_state::first_finishing)) {
        // We are first to arrive at completion.
        // Store the continuation to move past await.
        this->main_cont_ = await_cc;
        // We are done "finishing"
        sync_state_ = sync_state::first_finished;
        // TODO: thread_cont_ may not be set yet; there is a race condition
        return std::exchange(this->thread_cont_, nullptr);
      } else {
        // The async thread finished; we can continue directly.
        return await_cc;
      }
    });
    (void)c;
    // We are here if both threads finish; but we don't know which thread finished last and is
    // currently executing this.
  }

  void execute(int) noexcept {
    profiling::duplicate_zones_stack scoped_zones_stack{zones_};
    cont_ = detail::callcc([this](detail::continuation_t thread_cont) -> detail::continuation_t {
      thread_cont_ = thread_cont;
      res_ = f_();
      auto c = on_async_complete();
      if (c) {
        c = detail::resume(c);
      }
      return std::exchange(thread_cont_, nullptr);
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
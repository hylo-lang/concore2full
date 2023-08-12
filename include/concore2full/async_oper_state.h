#pragma once

#include "concore2full/detail/callcc.h"
#include "concore2full/detail/task.h"
#include "concore2full/global_thread_pool.h"

namespace concore2full {

/// @brief Object to store the state of an asyncrhonous operation, and interact with it.
///
/// @tparam T The return type of the asyncrhonous operation.
///
/// This is used to spawn new work concurrently and to await the completion of this work.
///
/// It will store the state of the asyncrhonous object. This object cannot be moved nor copied, and
/// has to remain valid for the entire async operation.
///
/// One can use `spawn` to start concurrent work for this operation state. One cannot start multiple
/// asyncrhonous operations on the same object.
///
/// A spawned operation can be awaited by using `await` method. This will ensure that the execution
/// is continue once the computaiton is finished. This doesn't block the current thread; if the
/// current thread arrives at the await point earlier, there is a "thread inversion" and the thread
/// will continue to work the coroutine.
///
/// If the object is destryoed before the spawned work finishes, the work will be cancelled.
template <typename T, typename Storage = int> class async_oper_state {
public:
  async_oper_state() = default;
  ~async_oper_state() = default;
  // No copy, no move
  async_oper_state(const async_oper_state&) = delete;

  /// @brief Await the result of the computation
  ///
  /// @return The result of the computation; throws if the operation was cancelled.
  ///
  /// If the main thread of execution arrives at this point after the work is done, this will simply
  /// return the result of the work. If the main thread of exection arrives here before the work is
  /// completed, a "thread inversion" happens, and the main thread of execution continues work on
  /// the coroutine that was just spawned.
  T await() {
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

  /// TODO
  Storage storage_;
  /// TODO: check if we can remove this.
  detail::continuation_t cont_;
  /// The location where we need to store the result
  T res_;
  /// The state of the computation, with respect to reaching the await point.
  std::atomic<sync_state> sync_state_{sync_state::both_working};
  /// Continuation pointing to the code after the `await`, on the main thread of execution.
  detail::continuation_t main_cont_;
  /// Continuation pointing to the end of the thread.
  detail::continuation_t thread_cont_;

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

  template <typename Fn> friend auto spawn(Fn&& f);

  template<typename Fn>
  async_oper_state(Fn&& to_execute) {
    to_execute(*this);
  }
};

/// @brief  Spawn work with the default scheduler
/// @tparam Fn The type of the function to execute
/// @param f The function representing the work that needs to be executed concurrently.
///
/// This will use the default scheduler to spawn new work concurrently.
///
/// TODO: `f` needs to return something convertivle to `T`.
template <typename Fn> inline auto spawn(Fn&& f) {
  using res_t = std::invoke_result_t<Fn>;
  using op_t = async_oper_state<res_t>;
  auto to_execute = [f = std::forward<Fn>(f)](op_t& op) {
    profiling::zone_stack_snapshot current_zones;
    auto f_cont = [&op, f = std::move(f)](
                      detail::continuation_t thread_cont) -> detail::continuation_t {
      op.thread_cont_ = thread_cont;
      op.res_ = f();
      auto c = op.on_async_complete();
      if (c) {
        c = detail::resume(c);
      }
      return std::exchange(op.thread_cont_, nullptr);
    };
    global_thread_pool().start_thread([&op, f_cont = std::move(f_cont), current_zones] {
      profiling::duplicate_zones_stack scoped_zones_stack{current_zones};
      op.cont_ = detail::callcc(std::move(f_cont));
    });
  };
  return op_t{std::move(to_execute)};
}

} // namespace concore2full
#pragma once

#include "detail/continuation.h"
#include "global_thread_pool.h"

namespace concore2full {

template <typename T> class async_oper_state {
public:
  async_oper_state() = default;
  ~async_oper_state() = default;
  async_oper_state(const async_oper_state&) = delete;
  async_oper_state(async_oper_state&&) = delete;

  template <typename Fn> void spawn(Fn&& f) {
    profiling::zone_stack_snapshot current_zones;
    auto f_cont = [this, f = std::forward<Fn>(f)](
                      context::continuation_t thread_cont) -> context::continuation_t {
      this->thread_cont_ = thread_cont;
      res_ = f();
      auto c = this->on_async_complete();
      if (c) {
        c = context::resume(c);
      }
      return std::exchange(this->thread_cont_, nullptr);
    };
    global_thread_pool().start_thread([this, f_cont = std::move(f_cont), current_zones] {
      profiling::duplicate_zones_stack scoped_zones_stack{current_zones};
      this->cont_ = context::callcc(std::move(f_cont));
    });
  }

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

  context::continuation_t cont_;
  T res_;
  std::atomic<sync_state> sync_state_{sync_state::both_working};
  context::continuation_t main_cont_;
  context::continuation_t thread_cont_;

  context::continuation_t on_async_complete() {
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

  void on_main_complete() {
    auto c = context::callcc([this](context::continuation_t await_cc) -> context::continuation_t {
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
};

} // namespace concore2full
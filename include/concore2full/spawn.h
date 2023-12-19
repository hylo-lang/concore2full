#pragma once

#include "concore2full/c/spawn.h"
#include "concore2full/c/task.h"
#include "concore2full/detail/callcc.h"
#include "concore2full/detail/thread_switch_helper.h"
#include "concore2full/global_thread_pool.h"
#include "concore2full/this_thread.h"

#include <type_traits>

namespace concore2full {

namespace detail {

template <typename T> struct value_holder {
  using value_t = std::remove_cvref_t<T>;

  value_holder() = default;

  value_t& value() noexcept { return value_; }

private:
  value_t value_;
};

template <> struct value_holder<void> {
  using value_t = void;

  value_holder() = default;

  void value() noexcept {}
};

void on_main_complete(concore2full_spawn_frame* frame);

//! Holds core spawn frame, the spawn function and the result of the spawn function.
template <typename Fn>
struct full_spawn_frame : concore2full_spawn_frame, value_holder<std::invoke_result_t<Fn>> {
  Fn f_;

  using value_holder_t = detail::value_holder<std::invoke_result_t<Fn>>;
  using res_t = typename value_holder_t::value_t;

  explicit full_spawn_frame(Fn&& f) : f_(std::forward<Fn>(f)) {
    concore2full_initialize(this, &to_execute);
  }

  full_spawn_frame(full_spawn_frame&& other) : f_(std::move(other.f_)) {
    concore2full_initialize(this, &to_execute);
  }

private:
  static void to_execute(concore2full_spawn_frame* frame) noexcept {
    auto* d = static_cast<detail::full_spawn_frame<Fn>*>(frame);

    if constexpr (std::is_same_v<res_t, void>) {
      std::invoke(std::forward<Fn>(d->f_));
    } else {
      static_cast<value_holder_t*>(d)->value() = std::invoke(std::forward<Fn>(d->f_));
    }
  }
};

} // namespace detail

/**
 * @brief The state of a spawned execution.
 * @tparam Fn The type of the functor used for spawning concurrent work.
 *
 * This represents the result of a `spawn` call, containing the state associated with the spawned
 * work. It will hold the data needed to execute the spawned computation and get its results.
 *
 * It can only be constructed through calling the `spawn()` function.
 *
 * It provides mechanisms for the main thread of execution to *await* the finish of the
 * computation. That is, continue when the computation is finished. If the main thread of execution
 * finishes first, then *thread inversion* will happen on the await point: the spawned thread will
 * continue the main work, while the main thread will be suspended. However, no OS thread will be
 * blocked.
 *
 * It exposes the type of the result of the spawned computation.
 *
 * If the object is destroyed before the spawned work finishes, the work will be cancelled.
 *
 * @sa spawn()
 */
template <typename Fn> class spawn_state {
public:
  ~spawn_state() = default;
  //! No copy, no move
  spawn_state(const spawn_state&) = delete;

  //! The type returned by the spawned computation.
  using res_t = typename detail::full_spawn_frame<Fn>::res_t;

  /**
   * @brief Await the result of the computation.
   * @return The result of the computation; throws if the operation was cancelled.
   *
   * If the main thread of execution arrives at this point after the work is done, this will simply
   * return the result of the work. If the main thread of execution arrives here before the work is
   * completed, a "thread inversion" happens, and the main thread of execution continues work on
   * the coroutine that was just spawned.
   */
  res_t await() {
    on_main_complete(&base_);
    return base_.value();
  }

private:
  //! The base operation state, stored in dynamic memory, so that we can move this object.
  detail::full_spawn_frame<Fn> base_;

  template <typename F> friend auto spawn(F&& f);

  //! Private constructor. `spawn` will call this.
  spawn_state(Fn&& f) : base_(std::forward<Fn>(f)) { global_thread_pool().enqueue(&base_.task_); }
};

//! Same as `spawn_state`, but allows the spawned function to escape the scope of the caller.
//! Used to implement weekly-structured concurrency.
template <typename Fn> class escaping_spawn_state {
public:
  ~escaping_spawn_state() = default;
  escaping_spawn_state(const escaping_spawn_state&) = default;
  escaping_spawn_state(escaping_spawn_state&&) = default;

  //! The type returned by the spawned computation.
  using res_t = typename detail::full_spawn_frame<Fn>::res_t;

  /**
   * @brief Await the result of the computation.
   * @return The result of the computation; throws if the operation was cancelled.
   *
   * If the main thread of execution arrives at this point after the work is done, this will simply
   * return the result of the work. If the main thread of execution arrives here before the work is
   * completed, a "thread inversion" happens, and the main thread of execution continues work on
   * the coroutine that was just spawned.
   */
  res_t await() {
    on_main_complete(base_.get());
    return base_->value();
  }

private:
  using base_t = detail::full_spawn_frame<Fn>;

  //! The base operation state, stored in dynamic memory, so that we can move this object.
  std::shared_ptr<base_t> base_;

  template <typename F> friend auto escaping_spawn(F&& f);

  //! Private constructor. `spawn` will call this.
  escaping_spawn_state(Fn&& f) : base_(std::make_shared<base_t>(base_t{std::forward<Fn>(f)})) {
    global_thread_pool().enqueue(&base_->task_);
  }
};

/**
 * @brief Spawn work with the default scheduler.
 * @tparam Fn The type of the function to execute.
 * @param f The function representing the work that needs to be executed asynchronously.
 * @return A `spawn_state` object; this object cannot be copied or moved
 *
 * This will use the default scheduler to spawn new work concurrently.
 *
 * The returned state object needs to stay alive for the entire duration of the computation. If the
 * object is destructed, the computation is cancelled.
 */
template <typename Fn> inline auto spawn(Fn&& f) {
  using op_t = spawn_state<Fn>;
  return op_t{std::forward<Fn>(f)};
}

template <typename Fn> inline auto escaping_spawn(Fn&& f) {
  using op_t = escaping_spawn_state<Fn>;
  return op_t{std::forward<Fn>(f)};
}

} // namespace concore2full
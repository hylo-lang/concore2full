#pragma once

#include "concore2full/detail/context_function.h"
#include "concore2full/detail/stack_control_structure.h"

#include "concore2full/profiling.h"
#include "concore2full/stack/stack_allocator.h"

namespace concore2full {
namespace detail {

/// @brief Allocate memory to be used as stack by a stackfull coroutine around the given function.
/// @param allocator The allocator to be used
/// @param f The main function of a coroutine.
/// @return The control structure for the coroutine.
inline auto allocate_stack(stack::stack_allocator auto&& allocator, context_function auto&& f) {
  using control_t = stack_control_structure<decltype(allocator), decltype(f)>;
  // Allocate the stack.
  stack::stack_t stack = allocator.allocate();
  // Put the control structure on the stack, at the end of the allocated space.
  uintptr_t align = alignof(control_t);
  void* p = reinterpret_cast<void*>(
      (reinterpret_cast<uintptr_t>(stack.sp) - static_cast<uintptr_t>(sizeof(control_t))) & ~align);
  return new (p)
      control_t{stack, std::forward<decltype(allocator)>(allocator), std::forward<decltype(f)>(f)};
};

/// @brief Called to finish the execution in the coroutine
/// @tparam C The type of control structure we use for the coroutine
/// @param t The transfer object containing the control structure as data.
/// @return A transfer_t object with nulls.
///
/// We use this `(transfer_t) -> transfer_t` as this is meant to be passed to
/// `context_core_api_ontop_fcontext`.
///
/// After this is called, the coroutine will be destroyed.
template <typename C>
inline detail::transfer_t execution_context_exit(detail::transfer_t t) noexcept {
  auto control = reinterpret_cast<C*>(t.data);
  profiling::zone_instant{CURRENT_LOCATION()}.add_flow_terminate(reinterpret_cast<uint64_t>(control));
  destroy(control);
  return {nullptr, nullptr};
}

//! The entry point for a stack execution context.
//! Executes the *main function* (the one passed to `callcc`), and then destroys the execution
//! context.

/// @brief The entry point for a coroutine
/// @tparam C The type of the control structure.
/// @param t The calling continuation and data pointing to the control structure.
///
/// This will be the entry function for the coroutine. It is a wrapper on top of the function given
/// to create the coroutine, but also handles the destruction of the coroutine.
template <typename C> inline void execution_context_entry(detail::transfer_t t) noexcept {
  // The parameter passed in is our control structure.
  // cppcheck-suppress uninitvar
  auto* control = reinterpret_cast<C*>(t.data);
  assert(control);
  assert(t.fctx);
  {
    profiling::zone zone{CURRENT_LOCATION_N("coro.execute")};
    zone.add_flow(reinterpret_cast<uint64_t>(control));
    zone.set_param("ctx", as_value(t.fctx));

    // Start executing the given function.
    t.fctx = std::invoke(control->main_function_, t.fctx);
    assert(t.fctx);
  }

  // Destroy the stack context.
  context_core_api_ontop_fcontext(t.fctx, control, execution_context_exit<C>);
  // Should never reach this point.
  assert(false);
}

//! Creates an execution context, and starts executing the given function.
//! Returns the continuation handle returned from the function.

/// @brief Create a stackfull coroutine and starts executing it.
/// @param allocator The allocator object to be used to allocate the coroutine stack.
/// @param f The function to be run in the new coroutine
/// @return Continuation object to the point where the control is passed back to the caller.
///
/// This will create a stackfull coroutine around `f`, using `allocator` to create the needed stack.
/// It will start executing the function, passing a continuation to the parent as a parameter to the
/// function. The function may resume this continuation, and, in that case, this function will
/// resume returning the continuation object to the point that started the resumption.
inline continuation_t create_stackfull_coroutine(stack::stack_allocator auto&& allocator,
                                                 context_function auto&& f) {
  auto* control =
      allocate_stack(std::forward<decltype(allocator)>(allocator), std::forward<decltype(f)>(f));
  char name[32];
  snprintf(name, sizeof(name), "coro-%p", control->stack_begin());
  profiling::define_stack(control->stack_begin(), control->stack_end(), name);
  profiling::zone_instant{CURRENT_LOCATION_N("callcc.make_fcontext")}.add_flow(as_value(control));

  // Create a context for running the new code.
  using C = std::decay_t<decltype(*control)>;
  continuation_t ctx = context_core_api_make_fcontext(control->stack_end(), control->useful_size(),
                                                      execution_context_entry<C>);
  assert(ctx != nullptr);
  return context_core_api_jump_fcontext(ctx, control).fctx;
}

} // namespace detail
} // namespace concore2full

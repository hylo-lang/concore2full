#pragma once

#include "concore2full/detail/context_function.h"
#include "concore2full/detail/core_types.h"
#include "concore2full/detail/create_stackfull_coroutine.h"
#include "concore2full/profiling.h"
#include "concore2full/stack/simple_stack_allocator.h"
#include "concore2full/stack/stack_allocator.h"

#include <context_core_api.h>

namespace concore2full {
namespace detail {

inline continuation_t callcc(std::allocator_arg_t, stack::stack_allocator auto&& salloc,
                             context_function auto&& f);

/// @brief Call with current continuation.
/// @param std::allocator_arg_t Tag used to specify that we are providing an allocator
/// @param salloc The stack allocator object to be used
/// @param f The function to be called, passing in the current continuation.
///
/// @return A continuation to the point that resumed execution for this control flow
///
/// This will create a stackfull coroutine to execute the given function.
///
/// This will suspend the current execution control flow, saving a "continuation" object pointing to
/// this suspension point. The given function will be called by passing this "continuation" object.
///
/// The given function is expected eventually to resume the continuation passed in. At that point, a
/// continuation object is retained, and it will be returned by this function call. If the given
/// function returns without resuming to the context, then this function will resume returning
/// `nullptr`.
///
/// The return continuation of the given function will be used to call the destruction of the
/// stackfull coroutine.
///
/// If stack allocator is not provided, a default `simple_stack_allocator` will be used.
///
/// @sa resume()
inline continuation_t callcc(std::allocator_arg_t, stack::stack_allocator auto&& salloc,
                             context_function auto&& f) {
  profiling::zone zone{CURRENT_LOCATION()};
  return detail::create_stackfull_coroutine(std::forward<decltype(salloc)>(salloc),
                                            std::forward<decltype(f)>(f));
}
inline continuation_t callcc(context_function auto&& f) {
  return callcc(std::allocator_arg, stack::simple_stack_allocator(), std::forward<decltype(f)>(f));
}

//! Resumes the given continuation.
//! The current execution is interrupted, and the program continues from the given continuation
//! point. Returns the context that has been suspended.

/// @brief Resumes the given continuation.
///
/// @param continuation Handle to the point we want to resume.
/// @return The continuation to the control flow that resumes back the execution to the current
/// control flow.
///
/// This is equivalent to:
///   - suspend the current control flow; save continuation point `c1`
///   - resumue the execution to the control flow indicated by `continuation` (a `resume` call)
///   - the `resume()` call that will continue will return `c1`
inline continuation_t resume(continuation_t continuation) {
  profiling::zone zone{CURRENT_LOCATION()};
  assert(continuation);
  return context_core_api_jump_fcontext(continuation, nullptr).fctx;
}

} // namespace detail
} // namespace concore2full

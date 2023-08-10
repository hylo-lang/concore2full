#pragma once

#include "context_function.h"
#include "core_types.h"
#include "create_stackfull_coroutine.h"
#include "profiling.h"
#include "simple_stack_allocator.h"
#include "stack_allocator.h"
#include <context_core_api.h>

namespace concore2full {
namespace detail {

inline continuation_t callcc(context_function auto&& f);
inline continuation_t callcc(std::allocator_arg_t, stack_allocator auto&& salloc,
                             context_function auto&& f);
inline continuation_t resume(continuation_t continuation);

//! Call with current continuation.
//! Takes the context of the code immediatelly following this function call, and passes it to the
//! given context function. The given function is executed in a new stack context. We can suspend
//! the context and resume other context, or the given context.
inline continuation_t callcc(context_function auto&& f) {
  return callcc(std::allocator_arg, simple_stack_allocator(), std::forward<decltype(f)>(f));
}
inline continuation_t callcc(std::allocator_arg_t, stack_allocator auto&& salloc,
                             context_function auto&& f) {
  // detail::profile_event_callcc();
  (void)profiling::zone{CURRENT_LOCATION_NC("callcc", profiling::color::green)};
  return detail::create_stackfull_coroutine(std::forward<decltype(salloc)>(salloc),
                                            std::forward<decltype(f)>(f));
}

//! Resumes the given continuation.
//! The current execution is interrupted, and the program continues from the given continuation
//! point. Returns the context that has been suspended.
inline continuation_t resume(continuation_t continuation) {
  (void)profiling::zone{CURRENT_LOCATION_NC("resume", profiling::color::green)}.set_value(
      detail::as_value(continuation));
  assert(continuation);
  return context_core_api_jump_fcontext(continuation, nullptr).fctx;
}

} // namespace detail
} // namespace concore2full

#pragma once

#include "context_function.h"
#include "core_types.h"
#include "stack_allocator.h"

namespace concore2full {
namespace detail {

//! The constrol structure that needs to be placed on a stack to be able to use it for stackfull
//! coroutines. We need to know how to deallocate the stack memory, and we also need to store the
//! data for the main function to be run on this stack.
template <stack_allocator S, context_function F> struct stack_control_structure {
  //! The stack we are operating on.
  stack_t stack_;
  //! The allocator used to create the stack, and to deallocate it.
  std::decay_t<S> allocator_;
  //! The main function to run in this new context.
  std::decay_t<F> main_function_;

  friend void destroy(stack_control_structure* record) {
    // Save needed data.
    S allocator = std::move(record->allocator_);
    stack_t stack = record->stack_;
    // Destruct the object.
    record->~stack_control_structure();
    // Destroy the stack.
    allocator.deallocate(stack);
  }

  //! The end of the useful portion of the stack.
  void* stack_end() const noexcept {
    // Create a 64-byte gap between the control structure and the usefule stack.
    constexpr uintptr_t gap = 64;
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(this) - gap);
  }
  //! The begin of the useful portion of the stack.
  void* stack_begin() const noexcept {
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(stack_.sp) -
                                   static_cast<uintptr_t>(stack_.size));
  }
  //! The useful size of the stack (where the executing code can store data).
  uintptr_t useful_size() const noexcept {
    return reinterpret_cast<uintptr_t>(stack_end()) - reinterpret_cast<uintptr_t>(stack_begin());
  }
};

} // namespace detail
} // namespace concore2full
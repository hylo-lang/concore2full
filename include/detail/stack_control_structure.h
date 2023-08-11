#pragma once

#include "context_function.h"
#include "core_types.h"
#include "stack/stack_allocator.h"

#include <cassert>

namespace concore2full {
namespace detail {

/// @brief The control structure we put on stack for creating a stackfull coroutine.
///
/// @tparam S The type of the stack allocator we use
/// @tparam F The type of the main context function we are using.
///
/// This will be placed on the stack to be able to start the coroutine. It contains information on
/// how to deallocate the coroutine stack and it keeps track of the coroutine function object.
template <stack::stack_allocator S, context_function F> struct stack_control_structure {
  /// The stack we are operating on.
  stack::stack_t stack_;
  /// The allocator used to create the stack, and to deallocate it.
  std::decay_t<S> allocator_;
  /// The main function to run in this new context.
  std::decay_t<F> main_function_;

  /// @brief  Destroys the stackfull coroutine.
  /// @param record Pointer to this, indicating the coroutine to be destroyed.
  ///
  /// This will destroy this object and deallocate the stack.
  friend void destroy(stack_control_structure* record) {
    // Save needed data.
    S allocator = std::move(record->allocator_);
    stack::stack_t stack = record->stack_;
    // Destruct the object.
    record->~stack_control_structure();
    // Destroy the stack.
    allocator.deallocate(stack);
  }

  /// The end of the useful portion of the stack.
  void* stack_end() const noexcept {
    // Create a 64-byte gap between the control structure and the useful stack.
    constexpr uintptr_t gap = 64;
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(this) - gap);
  }
  /// The begin of the useful portion of the stack.
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
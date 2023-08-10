#pragma once

#include <detail/core_types.h>

namespace concore2full {
namespace stack {

//! The memory space for the stack of a new context
struct stack_t {
  std::size_t size{0};
  void* sp{nullptr};
};

/// @brief Concept for a stack allocator.
///
/// It knows how to allocate and deallocate a coroutine stack.
template <typename T>
concept stack_allocator = requires(T obj, stack_t stack) {
  { obj.allocate() } -> std::same_as<stack_t>;
  { obj.deallocate(stack) };
};

} // namespace stack
} // namespace concore2full
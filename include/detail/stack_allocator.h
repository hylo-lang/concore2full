#pragma once

#include "core_types.h"

namespace concore2full {
namespace detail {

/// @brief Concept for a stack allocator.
///
/// It knows how to allocate and deallocate a coroutine stack.
template <typename T>
concept stack_allocator = requires(T obj, stack_t stack) {
  { obj.allocate() } -> std::same_as<stack_t>;
  { obj.deallocate(stack) };
};

} // namespace detail
} // namespace concore2full
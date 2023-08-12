#pragma once

#include "concore2full/stack/stack_allocator.h"

namespace concore2full {
namespace stack {

/// @brief A simple stack allocator that uses `malloc`.
///
/// Each time a new coroutine stack is needed, this will allocate a new block of memory and return
/// it.
///
/// The allocator can receive a size on constructor to be used when allocating stacks. If this size
/// is not provided, a default stack size will be used.

class simple_stack_allocator {
  std::size_t size_;

public:
  /// The default stack size
  static constexpr std::size_t default_size_ = 1024 * 1024;

  /// @brief Initializes the size to be used when allocating stacks.
  /// @param size The size to be used for allocating stack. Default = 1MB
  simple_stack_allocator(std::size_t size = default_size_) : size_(size) {}

  /// @brief Allocate a stack to be used for coroutines.
  /// @return Details about the newly allocated stack memory.
  stack_t allocate() {
    void* mem = std::malloc(size_);
    if (!mem)
      throw std::bad_alloc();
    return {size_, static_cast<char*>(mem) + size_};
  }
  /// @brief Deallocate the stack memory.
  /// @param stack Object indicating the stack that needs to be deallocated.
  void deallocate(stack_t stack) {
    void* mem = static_cast<char*>(stack.sp) - stack.size;
    std::free(mem);
  }
};

} // namespace stack
} // namespace concore2full
#pragma once

#include "core_types.h"

namespace concore2full {
namespace detail {

class simple_stack_allocator {
  std::size_t size_;

public:
  static constexpr std::size_t default_size_ = 1024 * 1024;

  simple_stack_allocator(std::size_t size = default_size_) : size_(default_size_) {}

  detail::stack_t allocate() {
    void* mem = std::malloc(size_);
    if (!mem)
      throw std::bad_alloc();
    return {size_, static_cast<char*>(mem) + size_};
  }
  void deallocate(detail::stack_t stack) {
    void* mem = static_cast<char*>(stack.sp) - stack.size;
    std::free(mem);
  }
};


} // namespace detail
} // namespace concore2full
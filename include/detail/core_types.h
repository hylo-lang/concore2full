#pragma once

#include "profiling.h"
#include <context_core_api.h>

#include <cassert>
#include <functional>

namespace concore2full {
namespace detail {

//! A handle for a continuation.
//! This can be thought as a point from which we can susoend and resume exection of the program.
using continuation_t = context_core_api_fcontext_t;

//! The type of object used for transferring a conttinuation handle with some data.
using transfer_t = context_core_api_transfer_t;

//! The memory space for the stack of a new context
struct stack_t {
  std::size_t size{0};
  void* sp{nullptr};
};

//! Returns a value from the continuation; to be used in profiling.
uint64_t as_value(continuation_t c) {
  return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(c));
}

} // namespace detail
} // namespace concore2full

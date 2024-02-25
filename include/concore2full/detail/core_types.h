#pragma once

#include "concore2full/profiling.h"

#include <context_core_api.h>

#include <cassert>
#include <functional>

namespace concore2full {
namespace detail {

/// A handle for a continuation.
/// This can be thought as a point from which we can suspend and resume execution of the program.
using continuation_t = context_core_api_fcontext_t;

/// The type of object used for transferring a continuation handle with some data.
using transfer_t = context_core_api_transfer_t;

/// Returns a value from the continuation; to be used in profiling.
inline uint64_t as_value(continuation_t c) {
  return static_cast<uint64_t>(reinterpret_cast<uintptr_t>(c));
}

} // namespace detail
} // namespace concore2full

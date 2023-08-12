#pragma once

#include "concore2full/detail/core_types.h"

#include <functional>

namespace concore2full {
namespace detail {

/// Concept for the context-switching function types.
/// This matches all invocables with the signature `(continuation_t) -> continuation_t`.
template <typename F>
concept context_function = requires(F&& f, continuation_t continuation) {
  { std::invoke(std::forward<F>(f), continuation) } -> std::same_as<continuation_t>;
};

} // namespace detail
} // namespace concore2full
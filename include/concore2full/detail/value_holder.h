#pragma once

#include <type_traits>

namespace concore2full::detail {

template <typename T> struct value_holder {
  using value_t = std::remove_cvref_t<T>;

  value_holder() = default;

  value_t& value() noexcept { return value_; }

private:
  value_t value_;
};

template <> struct value_holder<void> {
  using value_t = void;

  value_holder() = default;

  void value() noexcept {}
};

} // namespace concore2full::detail
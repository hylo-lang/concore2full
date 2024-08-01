#pragma once

namespace concore2full::detail {

template <class T> struct raw_delete {
  raw_delete() {}

  template <class U> raw_delete(const raw_delete<U>&) noexcept {}

  void operator()(T* ptr) const noexcept { operator delete(ptr); }
};

template <typename T> using raw_unique_ptr = std::unique_ptr<T, raw_delete<T>>;

} // namespace concore2full::detail
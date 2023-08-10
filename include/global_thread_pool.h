#pragma once

#include "thread_pool.h"

namespace concore2full {

inline thread_pool& global_thread_pool() {
  static thread_pool instance;
  return instance;
}

} // namespace concore2full
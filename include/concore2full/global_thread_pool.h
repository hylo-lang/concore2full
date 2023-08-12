#pragma once

#include "concore2full/thread_pool.h"

namespace concore2full {

inline thread_pool& global_thread_pool() {
  static thread_pool instance;
  return instance;
}

} // namespace concore2full
#pragma once

#include "concore2full/detail/task.h"
#include "concore2full/profiling.h"

#include <thread>
#include <vector>

namespace concore2full {

class thread_pool {
public:
  template <typename Fn> void start_thread(Fn&& f) {
    threads_.emplace_back([f = std::forward<Fn>(f)] {
      profiling::set_cur_thread_name("worker thread");
      f();
    });
  }

  /// @brief  Enqueue a task for execution.
  /// @param task The task to be executed.
  void enqueue(detail::task_base* task) noexcept {
    threads_.emplace_back([task] {
      profiling::set_cur_thread_name("worker thread");
      task->execute();
    });
  }

  void clear() {
    for (auto& t : threads_) {
      t.join();
    }
    threads_.clear();
  }

private:
  std::vector<std::thread> threads_;
};

} // namespace concore2full
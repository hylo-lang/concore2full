#include "concore2full/thread_pool.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <latch>

using namespace std::chrono_literals;

template <std::invocable Fn> struct fun_task : concore2full::detail::task_base {
  Fn f_;
  explicit fun_task(Fn&& f) : f_(std::forward<Fn>(f)) {}

  void execute(int) noexcept override { std::invoke(f_); }
};

TEST_CASE("thread_pool can be default constructed, and has some parallelism", "[thread_pool]") {
  // Act
  concore2full::thread_pool sut;

  // Assert
  REQUIRE(sut.available_parallelism() > 1);
}
TEST_CASE("thread_pool can be default constructed with specified number of threads",
          "[thread_pool]") {
  // Act
  concore2full::thread_pool sut(13);

  // Assert
  REQUIRE(sut.available_parallelism() == 13);
}
TEST_CASE("thread_pool can execute tasks", "[thread_pool]") {
  // Arrange
  concore2full::thread_pool sut;
  bool called{false};
  fun_task task{[&called] { called = true; }};

  // Act
  sut.enqueue(&task);
  sut.request_stop();
  sut.join();

  // Assert
  REQUIRE(called);
}
TEST_CASE("thread_pool can execute two tasks in parallel", "[thread_pool]") {
  // Arrange
  concore2full::thread_pool sut;
  std::latch l{2};
  bool called1{false};
  bool called2{false};
  fun_task task1{[&called1, &l] {
    l.arrive_and_wait();
    called1 = true;
  }};
  fun_task task2{[&called2, &l] {
    l.arrive_and_wait();
    called2 = true;
  }};

  // Act
  sut.enqueue(&task1);
  sut.enqueue(&task2);
  sut.request_stop();
  sut.join();

  // Assert
  REQUIRE(called1);
  REQUIRE(called2);
}

TEST_CASE("thread_pool can execute tasks in parallel, to the availabile hardware concurrency",
          "[thread_pool]") {
  /*
  Notes on test implementation:
    - We have n threads, and several times more tasks to complete.
    - Each task waits for at least `n` other tasks the be started before they do their work.
    - Ideally, if all the tasks are evenly distributed to all the threads, we should be fine with
  just `n` tasks.
    - However, the tasks are not evenly distributed to the threads; we may have threads that get
  more than one task to execute. This is why we create more tasks that threads.
    - We get the tasks not distributed uniformly because we might have contention on thread's tasks
  list. (i.e., thread is starting, we enqueue work onto it, we try to execute work from it)
    - After the first set of tasks are running (and probably waiting), we take a small pause between
  the enqueueing of new tasks. This way, we reduce contention, and we ensure that we distribute the
  later tasks to all the threads.
  */
  concore2full::thread_pool sut;
  auto n = sut.available_parallelism();
  if (n > 2) {
    struct my_task : concore2full::detail::task_base {
      std::atomic<int>& task_counter_;
      int wait_limit_;
      bool called_{false};

      explicit my_task(std::atomic<int>& task_counter, int wait_limit)
          : task_counter_(task_counter), wait_limit_(wait_limit) {}

      void execute(int) noexcept override {
        // Wait until there are enough tasks executing; stop after some time, if we don't get the
        // required number of tasks entering here.
        task_counter_.fetch_add(1, std::memory_order_release);
        for (int i = 0; i < 10000; i++) {
          if (task_counter_.load(std::memory_order_acquire) >= wait_limit_) {
            // We are good
            called_ = true;
            break;
          } else {
            std::this_thread::sleep_for(100us);
          }
        }
      }
    };

    // Arrange
    std::atomic<int> task_counter{0};
    std::vector<my_task> tasks;
    int num_tasks = 3 * n;
    tasks.reserve(num_tasks);
    for (int i = 0; i < num_tasks; i++) {
      tasks.emplace_back(my_task{task_counter, n});
      std::this_thread::sleep_for(100us);
    }

    // Act
    for (auto& t : tasks) {
      sut.enqueue(&t);
    }
    sut.request_stop();
    sut.join();

    // Assert
    for (auto& t : tasks) {
      REQUIRE(t.called_);
    }
  }
}

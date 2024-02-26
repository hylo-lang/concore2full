#include "concore2full/thread_pool.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <latch>

using namespace std::chrono_literals;

template <std::invocable Fn> struct fun_task : concore2full_task {
  Fn f_;
  explicit fun_task(Fn&& f) : f_(std::forward<Fn>(f)) {
    task_function_ = &execute;
    next_ = nullptr;
  }

  static void execute(concore2full_task* task, int) noexcept {
    auto self = static_cast<fun_task*>(task);
    std::invoke(self->f_);
  }
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
  if (sut.available_parallelism() < 2)
    return;
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

TEST_CASE("thread_pool can execute tasks in parallel, to the available hardware concurrency",
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
    struct my_task : concore2full_task {
      std::atomic<int>& task_counter_;
      int wait_limit_;
      bool called_{false};

      explicit my_task(std::atomic<int>& task_counter, int wait_limit)
          : task_counter_(task_counter), wait_limit_(wait_limit) {
        task_function_ = &execute;
        next_ = nullptr;
      }

      static void execute(concore2full_task* task, int) noexcept {
        auto self = static_cast<my_task*>(task);
        // Wait until there are enough tasks executing; stop after some time, if we don't get the
        // required number of tasks entering here.
        self->task_counter_.fetch_add(1, std::memory_order_release);
        for (int i = 0; i < 10000; i++) {
          if (self->task_counter_.load(std::memory_order_acquire) >= self->wait_limit_) {
            // We are good
            self->called_ = true;
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

TEST_CASE("thread_pool can enqueue multiple tasks at once, and execute them", "[thread_pool]") {
  // Arrange
  concore2full::thread_pool sut;
  if (sut.available_parallelism() < 2)
    return;
  static constexpr int num_tasks = 29;
  std::atomic<int> count{0};
  struct my_task : concore2full_task {
    std::atomic<int>& count_;

    explicit my_task(std::atomic<int>& count) : count_(count) {
      task_function_ = &execute;
      next_ = nullptr;
    }

    static void execute(concore2full_task* task, int tid) noexcept {
      auto self = static_cast<my_task*>(task);
      self->count_++;
    }
  };
  std::vector<my_task> tasks;
  tasks.reserve(num_tasks);
  for (int i = 0; i < num_tasks; i++) {
    tasks.emplace_back(my_task{count});
  }

  // Act
  sut.enqueue_bulk(&tasks[0], num_tasks);
  sut.request_stop();
  sut.join();

  // Assert
  REQUIRE(count.load() == num_tasks);
}

#include "concore2full/thread_pool.h"

#include <catch2/catch_test_macros.hpp>

#include <atomic>
#include <chrono>
#include <functional>
#include <latch>

using namespace std::chrono_literals;

//! Wait until `predicate` returns true, using the pool-waiting technique.
//! Throws if `timeout` is reached.
void wait_until(std::predicate auto predicate, std::chrono::milliseconds sleep_time = 1ms,
                std::chrono::milliseconds timeout = 1s) {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  auto start_time = std::chrono::high_resolution_clock::now();
  while (true) {
    // If the predicate is true, we are done.
    if (predicate())
      return;
    // Check for timeout.
    if (std::chrono::high_resolution_clock::now() - start_time > timeout) {
      printf("Timeout\n");
      throw std::runtime_error("Timeout");
    }
    // Sleep for a while.
    std::this_thread::sleep_for(sleep_time);
  }
}

struct std_fun_task : concore2full_task {
  std::function<void()> f_;
  std_fun_task() = default;
  explicit std_fun_task(std::function<void()> f) : f_(std::move(f)) {
    task_function_ = &execute;
    next_ = nullptr;
  }

  static void execute(concore2full_task* task, int) noexcept {
    auto self = static_cast<std_fun_task*>(task);
    std::invoke(self->f_);
  }
};

//! Test that ensures that `pool` has at least `num_threads` parallelism.
void ensure_parallelism(concore2full::thread_pool& pool, int num_threads) {
  if (num_threads <= 2)
    return;

  // Arrange
  std::atomic<int> tasks_started{0};
  std::atomic<int> tasks_done{0};
  auto core_task_fun = [&tasks_started, &tasks_done, num_threads]() {
    (void)tasks_started.fetch_add(1, std::memory_order_release);
    wait_until([&] { return tasks_started.load(std::memory_order_acquire) >= num_threads; });
    (void)tasks_done.fetch_add(1, std::memory_order_release);
  };
  int num_tasks = 3 * num_threads;
  std::vector<std_fun_task> tasks;
  tasks.reserve(num_tasks);
  for (int i = 0; i < num_tasks; i++) {
    tasks.emplace_back(std::function<void()>([&core_task_fun] { core_task_fun(); }));
  }

  // Act
  for (auto& t : tasks) {
    pool.enqueue(&t);
  }

  // Assert
  wait_until([&] { return tasks_done.load() >= num_tasks; });
}

TEST_CASE("thread_pool can be default constructed, and has some parallelism", "[thread_pool]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  // Act
  concore2full::thread_pool sut;

  // Assert
  REQUIRE(sut.available_parallelism() > 1);
}
TEST_CASE("thread_pool can be default constructed with specified number of threads",
          "[thread_pool]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  // Act
  concore2full::thread_pool sut(13);

  // Assert
  REQUIRE(sut.available_parallelism() == 13);
}
TEST_CASE("thread_pool can execute tasks", "[thread_pool]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  // Arrange
  concore2full::thread_pool sut;
  bool called{false};
  std_fun_task task{[&called] { called = true; }};

  // Act
  sut.enqueue(&task);
  wait_until([&] { return called; });
  sut.join();

  // Assert
  REQUIRE(called);
}
TEST_CASE("thread_pool can execute two tasks in parallel", "[thread_pool]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  // Arrange
  concore2full::thread_pool sut;
  if (sut.available_parallelism() < 2)
    return;
  std::latch l{3};
  bool called1{false};
  bool called2{false};
  std_fun_task task1{[&called1, &l] {
    l.arrive_and_wait();
    called1 = true;
  }};
  std_fun_task task2{[&called2, &l] {
    l.arrive_and_wait();
    called2 = true;
  }};

  // Act
  sut.enqueue(&task1);
  sut.enqueue(&task2);
  l.arrive_and_wait();
  sut.join();

  // Assert
  REQUIRE(called1);
  REQUIRE(called2);
}

TEST_CASE("thread_pool can execute tasks in parallel, to the available hardware concurrency",
          "[thread_pool]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  // Arrange
  concore2full::thread_pool sut;

  // Act & Assert
  ensure_parallelism(sut, sut.available_parallelism());

  sut.join();
}

TEST_CASE("thread_pool can enqueue multiple tasks at once, and execute them", "[thread_pool]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
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
  wait_until([&] { return count.load(std::memory_order_relaxed) == num_tasks; });
  sut.join();

  // Assert
  REQUIRE(count.load() == num_tasks);
}

TEST_CASE("thread_pool allows another thread to help executing work", "[thread_pool]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  // Arrange
  concore2full::thread_pool sut(2);
  std::stop_source ss;
  auto thread_fun = [&] {
    concore2full::profiling::emit_thread_name_and_stack("extra-thread");
    sut.offer_help_until(ss.get_token());
  };
  std::thread extra_thread{thread_fun};

  // Act & Assert
  ensure_parallelism(sut, sut.available_parallelism() + 1);

  ss.request_stop();
  extra_thread.join();
  sut.join();
}

TEST_CASE("thread_pool allows multiple threads to help executing work", "[thread_pool]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  // Arrange
  concore2full::thread_pool sut(3);
  std::stop_source ss;
  auto thread_fun = [&] {
    concore2full::profiling::emit_thread_name_and_stack("extra-thread");
    sut.offer_help_until(ss.get_token());
  };
  std::thread extra_thread1{thread_fun};
  std::thread extra_thread2{thread_fun};
  std::thread extra_thread3{thread_fun};

  // Act & Assert
  ensure_parallelism(sut, sut.available_parallelism() + 3);

  ss.request_stop();
  extra_thread1.join();
  extra_thread2.join();
  extra_thread3.join();
  sut.join();
}

TEST_CASE("thread_pool still functions after a helping thread left the pool", "[thread_pool]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  // Arrange
  concore2full::thread_pool sut(3);
  std::stop_source ss;
  auto thread_fun = [&] {
    concore2full::profiling::emit_thread_name_and_stack("extra-thread");
    sut.offer_help_until(ss.get_token());
  };
  std::thread extra_thread{thread_fun};
  ensure_parallelism(sut, sut.available_parallelism() + 1);
  ss.request_stop();
  extra_thread.join();

  // Act & Assert
  ensure_parallelism(sut, sut.available_parallelism());

  sut.join();
}

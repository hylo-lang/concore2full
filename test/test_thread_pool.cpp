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
  auto start_time = std::chrono::high_resolution_clock::now();
  while (true) {
    // If the predicate is true, we are done.
    if (predicate())
      return;
    // Check for timeout.
    if (std::chrono::high_resolution_clock::now() - start_time > timeout)
      throw std::runtime_error("Timeout");
    // Sleep for a while.
    std::this_thread::sleep_for(sleep_time);
  }
}

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
  // Arrange
  std::atomic<int> task_counter{0};
  auto core_task_fun = [&task_counter, num_threads](int i) {
    task_counter.fetch_add(1, std::memory_order_release);
    wait_until([&] { return task_counter.load(std::memory_order_acquire) >= num_threads; });
  };
  int num_tasks = 3 * num_threads;
  std::vector<std_fun_task> tasks;
  tasks.reserve(num_tasks);
  for (int i = 0; i < num_tasks; i++) {
    tasks.emplace_back(std::function<void()>([&core_task_fun, i] { core_task_fun(i); }));
  }

  // Act
  for (auto& t : tasks) {
    pool.enqueue(&t);
  }

  // Assert
  wait_until([&] { return task_counter.load() >= num_tasks; });
}

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
  wait_until([&] { return called; });
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
  wait_until([&] { return called1 && called2; });
  sut.request_stop();
  sut.join();

  // Assert
  REQUIRE(called1);
  REQUIRE(called2);
}

TEST_CASE("thread_pool can execute tasks in parallel, to the available hardware concurrency",
          "[thread_pool]") {
  concore2full::thread_pool sut;
  if (sut.available_parallelism() < 2)
    return;
  ensure_parallelism(sut, sut.available_parallelism());
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
  wait_until([&] { return count.load() == num_tasks; });
  sut.request_stop();
  sut.join();

  // Assert
  REQUIRE(count.load() == num_tasks);
}

TEST_CASE("thread_pool allows another thread to help executing work", "[thread_pool]") {
  // Arrange
  concore2full::thread_pool sut(2);
  std::stop_source ss;
  std::thread extra_thread{[&] { sut.offer_help_until(ss.get_token()); }};

  // Act & Assert
  ensure_parallelism(sut, sut.available_parallelism() + 1);
  ss.request_stop();
  extra_thread.join();
}

TEST_CASE("thread_pool allows multiple threads to help executing work", "[thread_pool]") {
  // Arrange
  concore2full::thread_pool sut(2);
  std::stop_source ss;
  std::thread extra_thread1{[&] { sut.offer_help_until(ss.get_token()); }};
  std::thread extra_thread2{[&] { sut.offer_help_until(ss.get_token()); }};
  std::thread extra_thread3{[&] { sut.offer_help_until(ss.get_token()); }};

  // Act & Assert
  ensure_parallelism(sut, sut.available_parallelism() + 3);

  ss.request_stop();
  extra_thread1.join();
  extra_thread2.join();
  extra_thread3.join();
}

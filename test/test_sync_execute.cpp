#include "concore2full/spawn.h"
#include "concore2full/sync_execute.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <chrono>
#include <iostream>
#include <latch>
#include <random>
#include <thread>

using namespace std::chrono_literals;

template <typename Fn> int do_thread_inversion(Fn&& f) {
  auto h = concore2full::spawn([f = std::forward<Fn>(f)] {
    std::this_thread::sleep_for(500us);
    f();
    return 0;
  });
  std::this_thread::sleep_for(10us);
  return h.await(); // thread inversion
}

TEST_CASE("sync_execute can call a function", "[sync_execute]") {
  // Arrange
  bool called{false};
  // Act
  concore2full::sync_execute([&] { called = true; });
  // Assert
  REQUIRE(called);
}

TEST_CASE("sync_execute will finish on the same thread", "[sync_execute]") {
  // Arrange
  bool called{false};
  auto f = [&] { (void)do_thread_inversion([&] { called = true; }); };

  // Act
  auto tid1 = std::this_thread::get_id();
  concore2full::sync_execute(std::move(f));
  auto tid2 = std::this_thread::get_id();

  // Assert
  REQUIRE(tid1 == tid2);
  REQUIRE(called);
}

TEST_CASE("sync_execute will finish on the same thread after two thread inversions in a row",
          "[sync_execute]") {
  // Arrange
  bool called1{false};
  bool called2{false};
  auto f = [&] {
    (void)do_thread_inversion([&] { called1 = true; });
    (void)do_thread_inversion([&] { called2 = true; });
  };

  // Act
  auto tid1 = std::this_thread::get_id();
  concore2full::sync_execute(std::move(f));
  auto tid2 = std::this_thread::get_id();

  // Assert
  REQUIRE(tid1 == tid2);
  REQUIRE(called1);
  REQUIRE(called2);
}

TEST_CASE("sync_execute will finish on the same thread after two nested thread inversions",
          "[sync_execute]") {
  // Arrange
  bool called1{false};
  bool called2{false};
  auto f = [&] {
    (void)do_thread_inversion([&] {
      called1 = true;
      (void)do_thread_inversion([&] { called2 = true; });
    });
  };

  // Act
  auto tid1 = std::this_thread::get_id();
  concore2full::sync_execute(std::move(f));
  auto tid2 = std::this_thread::get_id();

  // Assert
  REQUIRE(tid1 == tid2);
  REQUIRE(called1);
  REQUIRE(called2);
}

TEST_CASE("nested sync_execute: two simple calls", "[sync_execute]") {
  // Arrange
  bool called1{false};
  bool called2{false};
  // Act
  auto tid1 = std::this_thread::get_id();
  concore2full::sync_execute([&] {
    called1 = true;
    concore2full::sync_execute([&] { called2 = true; });
  });
  auto tid2 = std::this_thread::get_id();

  // Assert
  REQUIRE(tid1 == tid2);
  REQUIRE(called1);
  REQUIRE(called2);
}

TEST_CASE("nested sync_execute: simple call + thread inversion", "[sync_execute]") {
  // Arrange
  bool called1{false};
  bool called2{false};
  // Act
  auto tid1 = std::this_thread::get_id();
  concore2full::sync_execute([&] {
    called1 = true;
    concore2full::sync_execute([&] { (void)do_thread_inversion([&] { called2 = true; }); });
  });
  auto tid2 = std::this_thread::get_id();

  // Assert
  REQUIRE(tid1 == tid2);
  REQUIRE(called1);
  REQUIRE(called2);
}

TEST_CASE("nested sync_execute: thread inversion + thread inversion", "[sync_execute]") {
  // Arrange
  bool called1{false};
  bool called2{false};
  // Act
  auto tid1 = std::this_thread::get_id();
  concore2full::sync_execute([&] {
    (void)do_thread_inversion([&] {
      called1 = true;
      concore2full::sync_execute([&] { (void)do_thread_inversion([&] { called2 = true; }); });
    });
  });
  auto tid2 = std::this_thread::get_id();

  // Assert
  REQUIRE(tid1 == tid2);
  REQUIRE(called1);
  REQUIRE(called2);
}

TEST_CASE("nested sync_execute: thread inversion + simple + thread inversion", "[sync_execute]") {
  // Arrange
  bool called1{false};
  bool called2{false};
  bool called3{false};
  // Act
  auto tid1 = std::this_thread::get_id();
  concore2full::sync_execute([&] {
    (void)do_thread_inversion([&] {
      called1 = true;
      concore2full::sync_execute([&] {
        called2 = true;
        concore2full::sync_execute([&] { (void)do_thread_inversion([&] { called3 = true; }); });
      });
    });
  });
  auto tid2 = std::this_thread::get_id();

  // Assert
  REQUIRE(tid1 == tid2);
  REQUIRE(called1);
  REQUIRE(called2);
  REQUIRE(called3);
}

TEST_CASE("sync_execute can return a value", "[sync_execute]") {
  // Act
  auto r = concore2full::sync_execute([&] { return 13; });

  // Assert
  REQUIRE(r == 13);
}

TEST_CASE("sync_execute can return a value in the presence of a thread switch", "[sync_execute]") {
  // Act
  auto r = concore2full::sync_execute([&] {
    int res = 0;
    (void)do_thread_inversion([&] { res = 13; });
    return res;
  });

  // Assert
  REQUIRE(r == 13);
}

TEST_CASE("sync_execute works in the presence of many thread switches", "[sync_execute]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  using concore2full::detail::callcc;
  using concore2full::detail::continuation_t;

  // Arrange
  constexpr int num_threads = 10;
  continuation_t continuations[num_threads];
  std::latch after_continuations_set{num_threads};
  std::latch before_hopping{1};
  auto work = [&](int index) {
    concore2full::sync_execute([&] {
      (void)callcc([&, index](continuation_t work_end) {
        concore2full::profiling::zone zone{CURRENT_LOCATION_N("work coro")};
        continuations[index] = work_end;
        after_continuations_set.count_down();
        // Wait for the main thread to shuffle the continuations
        before_hopping.wait();
        return continuations[index];
      });
    });
  };

  // Act
  std::vector<std::thread> threads;
  threads.reserve(num_threads);
  for (int i = 0; i < num_threads; i++) {
    threads.emplace_back([i, &work] { work(i); });
  }
  after_continuations_set.wait();
  std::shuffle(std::begin(continuations), std::end(continuations),
               std::mt19937{std::random_device{}()});
  before_hopping.count_down();
  for (auto& t : threads) {
    t.join();
  }

  // All should finish, without crashing or deadlocks
}

TEST_CASE("finishing multiple threads at once after sync_execute", "[sync_execute]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  using concore2full::detail::callcc;
  using concore2full::detail::continuation_t;

  // Arrange
  constexpr int num_threads = 10;
  continuation_t continuations[num_threads];
  std::latch after_continuations_set{num_threads};
  std::latch before_hopping{1};
  std::latch should_finish{1};
  auto work = [&](int index) {
    concore2full::profiling::zone zone{CURRENT_LOCATION_N("work")};
    concore2full::sync_execute([&] {
      (void)callcc([&, index](continuation_t work_end) {
        concore2full::profiling::zone zone{CURRENT_LOCATION_N("work coro")};
        continuations[index] = work_end;
        after_continuations_set.count_down();
        // Wait for the main thread to shuffle the continuations
        before_hopping.wait();
        return continuations[index];
      });
      should_finish.wait();
    });
  };

  // Act
  std::vector<std::thread> threads;
  threads.reserve(num_threads);
  for (int i = 0; i < num_threads; i++) {
    threads.emplace_back([i, &work] { work(i); });
  }
  after_continuations_set.wait();
  std::shuffle(std::begin(continuations), std::end(continuations),
               std::mt19937{std::random_device{}()});
  before_hopping.count_down();
  std::this_thread::sleep_for(500us);
  should_finish.count_down();
  for (auto& t : threads) {
    t.join();
  }

  // All should finish, without crashing or deadlocks
}

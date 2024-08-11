#include "concore2full/profiling.h"
#include "concore2full/spawn.h"
#include "concore2full/sync_execute.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <latch>
#include <semaphore>

using namespace std::chrono_literals;

TEST_CASE("spawn can execute work", "[spawn]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  // Arrange
  bool called{false};
  std::binary_semaphore done{0};

  // Act
  auto op{concore2full::spawn([&]() -> int {
    called = true;
    done.release();
    return 13;
  })};
  done.acquire();
  std::this_thread::sleep_for(5ms);
  auto res = op.await();

  // Assert
  REQUIRE(called);
  REQUIRE(res == 13);
}

TEST_CASE("spawn can execute work with void result", "[spawn]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  // Arrange
  bool called{false};
  std::binary_semaphore done{0};

  // Act
  auto op{concore2full::spawn([&] {
    called = true;
    done.release();
  })};
  done.acquire();
  op.await();

  // Assert
  REQUIRE(called);
}

TEST_CASE("spawn can execute a function that returns a reference", "[spawn]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  // Arrange
  bool called{false};
  std::binary_semaphore done{0};
  int x = 13;

  // Act
  auto op{concore2full::spawn([&]() -> int& {
    called = true;
    done.release();
    return x;
  })};
  done.acquire();
  int y = op.await();
  x = 17;

  // Assert
  REQUIRE(called);
  REQUIRE(y == 13);
}

TEST_CASE("escaping_spawn can execute work", "[spawn]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  // Arrange
  bool called{false};
  std::binary_semaphore done{0};

  // Act
  auto op{concore2full::escaping_spawn([&]() -> int {
    called = true;
    done.release();
    return 13;
  })};
  done.acquire();
  std::this_thread::sleep_for(5ms);
  auto res = op.await();

  // Assert
  REQUIRE(called);
  REQUIRE(res == 13);
}

TEST_CASE("escaping_spawn can execute work with void result", "[spawn]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  // Arrange
  bool called{false};
  std::binary_semaphore done{0};

  // Act
  auto op{concore2full::escaping_spawn([&] {
    called = true;
    done.release();
  })};
  done.acquire();
  op.await();

  // Assert
  REQUIRE(called);
}

TEST_CASE("escaping_spawn can execute a function that returns a reference", "[spawn]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  // Arrange
  bool called{false};
  std::binary_semaphore done{0};
  int x = 13;

  // Act
  auto op{concore2full::escaping_spawn([&]() -> int& {
    called = true;
    done.release();
    return x;
  })};
  done.acquire();
  int y = op.await();
  x = 17;

  // Assert
  REQUIRE(called);
  REQUIRE(y == 13);
}

auto create_op() {
  return concore2full::escaping_spawn([]() -> int { return 13; });
}

template <typename Op> auto receiver(Op op) { return op.await(); }

TEST_CASE("escaping_spawn result can be returned from functions", "[spawn]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  // Act
  auto future = create_op();
  auto res = receiver(std::move(future));

  // Assert
  REQUIRE(res == 13);
}

TEST_CASE("a copyable_spawn future can be copied", "[spawn]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};

  // Act
  auto f{concore2full::copyable_spawn([&]() -> int { return 13; })};
  auto f2 = f;
  auto f3 = f;

  // Assert
  REQUIRE(f.await() == 13);
  REQUIRE(f2.await() == 13);
  REQUIRE(f3.await() == 13);
}

TEST_CASE("copyable_spawn: multiple awaits while the task is not done yet", "[spawn]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  // Arrange
  std::latch l{4};
  int res1{-1};
  int res2{-1};
  int res3{-1};

  // Act
  auto f{concore2full::copyable_spawn([&]() -> int {
    concore2full::profiling::zone zone{CURRENT_LOCATION_N("task")};
    l.arrive_and_wait();
    return 13;
  })};
  auto t1 = std::thread([&l, &res1, f]() mutable {
    concore2full::profiling::zone zone{CURRENT_LOCATION_N("thread1")};
    concore2full::sync_execute([&l, &res1, f]() mutable {
      l.arrive_and_wait();
      res1 = f.await();
    });
  });
  auto t2 = std::thread([&l, &res2, f]() mutable {
    concore2full::profiling::zone zone{CURRENT_LOCATION_N("thread2")};
    concore2full::sync_execute([&l, &res2, f]() mutable {
      l.arrive_and_wait();
      res2 = f.await();
    });
  });
  auto t3 = std::thread([&l, &res3, f]() mutable {
    concore2full::profiling::zone zone{CURRENT_LOCATION_N("thread3")};
    concore2full::sync_execute([&l, &res3, f]() mutable {
      l.arrive_and_wait();
      res3 = f.await();
    });
  });
  t1.join();
  t2.join();
  t3.join();

  // Assert
  REQUIRE(res1 == 13);
  REQUIRE(res2 == 13);
  REQUIRE(res3 == 13);
}

TEST_CASE("copyable_spawn: multiple awaits unlocked by finishing the computation", "[spawn]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  // Arrange
  std::latch l{4};
  std::binary_semaphore can_finish{0};
  int res1{-1};
  int res2{-1};
  int res3{-1};

  // Act
  auto f{concore2full::copyable_spawn([&]() -> int {
    concore2full::profiling::zone zone{CURRENT_LOCATION_N("task")};
    can_finish.acquire();
    return 13;
  })};
  auto t1 = std::thread([&l, &res1, f]() mutable {
    concore2full::profiling::zone zone{CURRENT_LOCATION_N("thread1")};
    concore2full::sync_execute([&l, &res1, f]() mutable {
      l.arrive_and_wait();
      res1 = f.await();
    });
  });
  auto t2 = std::thread([&l, &res2, f]() mutable {
    concore2full::profiling::zone zone{CURRENT_LOCATION_N("thread2")};
    concore2full::sync_execute([&l, &res2, f]() mutable {
      l.arrive_and_wait();
      res2 = f.await();
    });
  });
  auto t3 = std::thread([&l, &res3, f]() mutable {
    concore2full::profiling::zone zone{CURRENT_LOCATION_N("thread3")};
    concore2full::sync_execute([&l, &res3, f]() mutable {
      l.arrive_and_wait();
      res3 = f.await();
    });
  });
  l.arrive_and_wait(); // all the threads reached the await point
  std::this_thread::sleep_for(100us);
  can_finish.release(); // let the task finish
  t1.join();
  t2.join();
  t3.join();

  // Assert
  REQUIRE(res1 == 13);
  REQUIRE(res2 == 13);
  REQUIRE(res3 == 13);
}

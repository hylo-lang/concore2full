#include "concore2full/spawn.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <semaphore>

using namespace std::chrono_literals;

TEST_CASE("spawn can execute work", "[spawn]") {
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

template <typename Op>
auto receiver(Op op) {
  return op.await();
}

TEST_CASE("escaping_spawn result can be returned from functions", "[spawn]") {
  // Act
  auto future = create_op();
  auto res = receiver(std::move(future));

  // Assert
  REQUIRE(res == 13);
}

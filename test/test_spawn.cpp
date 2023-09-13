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

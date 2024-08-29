#include "concore2full/spawn.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <latch>
#include <thread>

using namespace std::chrono_literals;

TEST_CASE("bulk_spawn can execute work", "[bulk_spawn]") {
  // Arrange
  static constexpr int count = 3;
  bool called[count] = {false, false, false};
  std::latch done{count};

  // Act
  auto op{concore2full::bulk_spawn(3, [&](int index) {
    called[index] = true;
    done.count_down();
  })};
  done.wait();
  std::this_thread::sleep_for(5ms);
  op.await();

  // Assert
  for (int i = 0; i < count; i++) {
    REQUIRE(called[i]);
  }
}

auto create_op(int count, std::atomic<int>& sum) {
  return concore2full::bulk_spawn(count, [&sum](int index) { sum += index; });
}

template <typename Op> void receiver(Op&& op) { std::forward<Op>(op).await(); }

TEST_CASE("bulk_spawn result can be returned from functions", "[bulk_spawn]") {
  // Arrange
  std::atomic<int> sum{0};
  // Act
  auto future = create_op(10, sum);
  receiver(std::move(future));

  // Assert
  REQUIRE(sum.load() == 45);
}

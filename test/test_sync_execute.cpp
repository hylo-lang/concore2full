#include "concore2full/spawn.h"
#include "concore2full/sync_execute.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

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
  std::thread::id tid1;
  std::thread::id tid2;
  bool called{false};
  auto f = [&] {
    auto h = concore2full::spawn([&] {
      std::this_thread::sleep_for(100ms);
      called = true;
      return 0;
    });
    std::this_thread::sleep_for(5ms);
    (void)h.await(); // thread inversion
  };

  // Act
  tid1 = std::this_thread::get_id();
  concore2full::sync_execute(std::move(f));
  tid2 = std::this_thread::get_id();

  // Assert
  REQUIRE(tid1 == tid2);
}

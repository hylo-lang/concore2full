#include "concore2full/spawn.h"
#include "concore2full/sync_execute.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <iostream>
#include <thread>

using namespace std::chrono_literals;

template <typename Fn> int do_thread_inversion(Fn&& f) {
  auto h = concore2full::spawn([f = std::forward<Fn>(f)] {
    std::this_thread::sleep_for(5ms);
    f();
    return 0;
  });
  std::this_thread::sleep_for(1ms);
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

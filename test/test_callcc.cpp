#include "detail/callcc.h"

#include <catch2/catch_test_macros.hpp>

#include <semaphore>
#include <thread>

using namespace concore2full;
using detail::callcc;
using detail::continuation_t;
using detail::resume;

TEST_CASE("can use callcc to execute code", "[callcc]") {
  // Arrange
  bool called = false;

  // Act
  auto c1 = callcc([&called](continuation_t& c) -> continuation_t {
    called = true;
    return c;
  });

  // Assert
  REQUIRE(called);
  REQUIRE(c1 == nullptr);
}

TEST_CASE("callcc can resume to the main continuation", "[callcc]") {
  // Arrange
  bool point1_touched = false;
  bool point2_touched = false;

  // Act/Assert
  auto c1 = callcc([&](continuation_t& c) -> continuation_t {
    point1_touched = true;
    c = resume(c);
    point2_touched = true;
    return c;
  });
  // Will be resumed shortly.
  REQUIRE(point1_touched);
  REQUIRE_FALSE(point2_touched);
  REQUIRE(c1 != nullptr);
  // Switch back to callcc function.
  c1 = resume(c1);
  // Will resume as soon as the function ends.
  REQUIRE(point1_touched);
  REQUIRE(point2_touched);
  REQUIRE(c1 == nullptr);
}

TEST_CASE("can switch between the execution of two callcc functions", "[callcc]") {
  // Arrange
  continuation_t c1{nullptr};
  continuation_t c2{nullptr};
  continuation_t parent{nullptr};
  bool fun1_done = false;
  bool fun2_done = false;

  // Act
  c1 = callcc([&](continuation_t& c) -> continuation_t {
    // Switch back asap.
    auto caller = resume(c);
    // Note: will be resume by the parent.
    parent = caller;
    // Switch to the second function.
    caller = resume(c2);
    // We are resumed again (now, from the first function).
    fun1_done = true;
    return caller;
  });
  c2 = callcc([&](continuation_t& c) -> continuation_t {
    // Switch back asap.
    auto caller = resume(c);
    // Note: will be resume by first function.
    // Switch to the first function.
    caller = resume(c1);
    // We are resumed again (by the first function).
    fun2_done = true;
    return parent;
  });
  auto r1 = resume(c1);

  // Assert
  REQUIRE(fun1_done);
  REQUIRE(fun2_done);
  REQUIRE(r1 == nullptr);
}

TEST_CASE("can use callcc to switch between threads", "[callcc]") {
  // Arrange
  std::binary_semaphore sem{0};
  continuation_t cont{nullptr};
  std::thread::id id1;
  std::thread::id id2;
  std::thread t{[&]() {
    // Wait until the continuation is valid.
    sem.acquire();
    // Jump to the continuation point.
    REQUIRE(cont != nullptr);
    auto caller = resume(cont);
    REQUIRE(caller == nullptr);
  }};

  // Act
  cont = callcc([&](continuation_t& c) -> continuation_t {
    // Note the thread this is started from
    id1 = std::this_thread::get_id();
    // Jump back to the parent immediatelly.
    c = resume(c);
    // Resumed from a different thread; note the thread ID
    id2 = std::this_thread::get_id();
    // Go back.
    return c;
  });
  // Tell the thread to call the coroutine.
  sem.release();
  // Wait for the thread to be done
  t.join();

  // Assert
  REQUIRE(id1 != id2);
}

#include "concore2full/spawn.h"
#include "concore2full/global_thread_pool.h"
#include "concore2full/profiling.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

int long_task(int input) {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  int result = input;
  for (int i = 0; i < 3; i++) {
    // concore2full::profiling::sleep_for(110ms);
    result += 1;
  }
  return result;
}

int greeting_task() {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  // std::cout << "Hello world! Have an int.\n";
  concore2full::profiling::sleep_for(130ms);
  return 13;
}

int concurrency_example() {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  auto op{concore2full::spawn([]() -> int { return long_task(0); })};
  auto x = greeting_task();
  auto y = op.await();
  return x + y;
}

TEST_CASE("simple example using concore2full", "[smoke]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  concore2full::profiling::sleep_for(100ms);
  /*int r =*/concurrency_example();
  // std::cout << r << "\n";

  concore2full::profiling::sleep_for(100ms);
  // std::cout << "expecting a crash here, while joining threads\n";
  // TODO: handle this gracefully
  concore2full::global_thread_pool().clear();
}

#include "async_oper_state.h"
#include "detail/profiling.h"
#include "global_thread_pool.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <iostream>

using namespace std::chrono_literals;

int long_task(int input) {
  profiling::zone zone{CURRENT_LOCATION()};
  int result = input;
  for (int i = 0; i < 3; i++) {
    // profiling::sleep_for(110ms);
    result += 1;
  }
  return result;
}

int greeting_task() {
  profiling::zone zone{CURRENT_LOCATION()};
  // std::cout << "Hello world! Have an int.\n";
  profiling::sleep_for(130ms);
  return 13;
}

int concurrency_example() {
  profiling::zone zone{CURRENT_LOCATION()};
  concore2full::async_oper_state<int> op;
  op.spawn([]() -> int { return long_task(0); });
  auto x = greeting_task();
  auto y = op.await();
  return x + y;
}

TEST_CASE("simple example using concore2full", "[smoke]") {
  profiling::zone zone{CURRENT_LOCATION()};
  profiling::sleep_for(100ms);
  int r = concurrency_example();
  // std::cout << r << "\n";

  profiling::sleep_for(100ms);
  // std::cout << "expecting a crash here, while joining threads\n";
  // TODO: handle this gracefully
  concore2full::global_thread_pool().clear();
}

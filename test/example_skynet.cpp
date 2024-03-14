#include "concore2full/global_thread_pool.h"
#include "concore2full/profiling.h"
#include "concore2full/spawn.h"
#include "concore2full/sync_execute.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <inttypes.h>

using namespace std::chrono_literals;

uint64_t skynet(int num, int size, int div) {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  zone.set_param("num", int64_t(num));
  zone.set_param("size", int64_t(size));
  if (size == 1) {
    concore2full::profiling::zone z1{CURRENT_LOCATION_N("skynet-1")};
    return uint64_t(num);
  } else {
    const int sub_size = size / div;
    int sub_num = num;
    auto f0 = concore2full::spawn([=] { return skynet(sub_num, sub_size, div); });
    sub_num += sub_size;
    auto f1 = concore2full::spawn([=] { return skynet(sub_num, sub_size, div); });
    sub_num += sub_size;
    auto f2 = concore2full::spawn([=] { return skynet(sub_num, sub_size, div); });
    sub_num += sub_size;
    auto f3 = concore2full::spawn([=] { return skynet(sub_num, sub_size, div); });
    sub_num += sub_size;
    auto f4 = concore2full::spawn([=] { return skynet(sub_num, sub_size, div); });
    sub_num += sub_size;
    auto f5 = concore2full::spawn([=] { return skynet(sub_num, sub_size, div); });
    sub_num += sub_size;
    auto f6 = concore2full::spawn([=] { return skynet(sub_num, sub_size, div); });
    sub_num += sub_size;
    auto f7 = concore2full::spawn([=] { return skynet(sub_num, sub_size, div); });
    sub_num += sub_size;
    auto f8 = concore2full::spawn([=] { return skynet(sub_num, sub_size, div); });
    sub_num += sub_size;
    auto f9 = concore2full::spawn([=] { return skynet(sub_num, sub_size, div); });

    uint64_t sum = 0;
    sum += f0.await();
    sum += f1.await();
    sum += f2.await();
    sum += f3.await();
    sum += f4.await();
    sum += f5.await();
    sum += f6.await();
    sum += f7.await();
    sum += f8.await();
    sum += f9.await();

    return sum;
  }
}

uint64_t run_benchmark() {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};
  // return skynet(0, 1000000, 10);
  return skynet(0, 1000, 10);
}

TEST_CASE("skynet microbenchmark example", "[benchmark]") {
  concore2full::profiling::emit_thread_name_and_stack("main");
  concore2full::profiling::zone zone{CURRENT_LOCATION()};

  auto now = std::chrono::high_resolution_clock::now();
  uint64_t result = run_benchmark();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::high_resolution_clock::now() - now);

  printf("Result: %" PRIu64 " in %d ms\n", result, int(duration.count()));
  REQUIRE(result == 499500);
}

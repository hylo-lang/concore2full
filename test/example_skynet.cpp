#include "concore2full/global_thread_pool.h"
#include "concore2full/profiling.h"
#include "concore2full/spawn.h"
#include "concore2full/sync_execute.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>
#include <inttypes.h>
#include <numeric>

using namespace std::chrono_literals;

uint64_t skynet_strict(int num, int size, int div) {
  // concore2full::profiling::zone zone{CURRENT_LOCATION()};
  // zone.set_param("num", int64_t(num));
  // zone.set_param("size", int64_t(size));
  if (size == 1) {
    // concore2full::profiling::zone z1{CURRENT_LOCATION_N("skynet-1")};
    return uint64_t(num);
  } else {
    const int sub_size = size / div;
    int sub_num = num;
    auto f0 = concore2full::spawn([=] { return skynet_strict(sub_num, sub_size, div); });
    sub_num += sub_size;
    auto f1 = concore2full::spawn([=] { return skynet_strict(sub_num, sub_size, div); });
    sub_num += sub_size;
    auto f2 = concore2full::spawn([=] { return skynet_strict(sub_num, sub_size, div); });
    sub_num += sub_size;
    auto f3 = concore2full::spawn([=] { return skynet_strict(sub_num, sub_size, div); });
    sub_num += sub_size;
    auto f4 = concore2full::spawn([=] { return skynet_strict(sub_num, sub_size, div); });
    sub_num += sub_size;
    auto f5 = concore2full::spawn([=] { return skynet_strict(sub_num, sub_size, div); });
    sub_num += sub_size;
    auto f6 = concore2full::spawn([=] { return skynet_strict(sub_num, sub_size, div); });
    sub_num += sub_size;
    auto f7 = concore2full::spawn([=] { return skynet_strict(sub_num, sub_size, div); });
    sub_num += sub_size;
    auto f8 = concore2full::spawn([=] { return skynet_strict(sub_num, sub_size, div); });
    sub_num += sub_size;
    auto f9 = concore2full::spawn([=] { return skynet_strict(sub_num, sub_size, div); });

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

uint64_t skynet_weak(int num, int size, int div);

struct skynet_weak_fun {
  int num_;
  int size_;
  int div_;

  uint64_t operator()() const { return skynet_weak(num_, size_, div_); }
};

uint64_t skynet_weak(int num, int size, int div) {
  // concore2full::profiling::zone zone{CURRENT_LOCATION()};
  // zone.set_param("num", int64_t(num));
  // zone.set_param("size", int64_t(size));
  if (size == 1) {
    // concore2full::profiling::zone z1{CURRENT_LOCATION_N("skynet-1")};
    return uint64_t(num);
  } else {
    using future_t = decltype(concore2full::escaping_spawn(skynet_weak_fun{0, 0, 0}));

    // Spawn the sub-tasks.
    const int sub_size = size / div;
    std::vector<future_t> futures;
    for (int i = 0; i < div; i++) {
      int sub_num = num + i * sub_size;
      futures.push_back(concore2full::escaping_spawn(skynet_weak_fun{sub_num, sub_size, div}));
    }

    // Wait for the results.
    uint64_t sum = 0;
    for (int i = 0; i < div; i++) {
      sum += futures[i].await();
    }

    return sum;
  }
}

uint64_t skynet_bulk(int num, int size, int div) {
  // concore2full::profiling::zone zone{CURRENT_LOCATION()};
  // zone.set_param("num", int64_t(num));
  // zone.set_param("size", int64_t(size));
  if (size == 1) {
    // concore2full::profiling::zone z1{CURRENT_LOCATION_N("skynet-1")};
    return uint64_t(num);
  } else {
    std::vector<uint64_t> results(div);

    // Spawn the sub-tasks, and put the results in the `results` array.
    const int sub_size = size / div;
    concore2full::bulk_spawn(div, [=, &results](int i) {
      int sub_num = num + i * sub_size;
      results[i] = skynet_bulk(sub_num, sub_size, div);
    }).await();

    // Get the sum of the results.
    return std::accumulate(results.begin(), results.end(), uint64_t(0));
  }
}

TEST_CASE("skynet microbenchmark example", "[benchmark]") {
  concore2full::profiling::emit_thread_name_and_stack("main");
  concore2full::profiling::zone zone{CURRENT_LOCATION()};

  auto now = std::chrono::high_resolution_clock::now();
  // uint64_t result = skynet_strict(0, 1'000'000, 10);
  uint64_t result = skynet_strict(0, 10'000, 10);
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::high_resolution_clock::now() - now);

  printf("Result: %" PRIu64 " in %d ms\n", result, int(duration.count()));
  REQUIRE(result == 49995000);
}

TEST_CASE("skynet microbenchmark example (weakly structured concurrency)", "[benchmark]") {
  concore2full::profiling::emit_thread_name_and_stack("main");
  concore2full::profiling::zone zone{CURRENT_LOCATION()};

  auto now = std::chrono::high_resolution_clock::now();
  // uint64_t result = skynet_weak(0, 1'000'000, 10);
  uint64_t result = skynet_weak(0, 10'000, 10);
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::high_resolution_clock::now() - now);

  printf("Result: %" PRIu64 " in %d ms\n", result, int(duration.count()));
  REQUIRE(result == 49995000);
}

TEST_CASE("skynet microbenchmark example (bulk_spawn)", "[benchmark]") {
  concore2full::profiling::emit_thread_name_and_stack("main");
  concore2full::profiling::zone zone{CURRENT_LOCATION()};

  auto now = std::chrono::high_resolution_clock::now();
  // uint64_t result = skynet_bulk(0, 1'000'000, 10);
  uint64_t result = skynet_bulk(0, 10'000, 10);
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::high_resolution_clock::now() - now);

  printf("Result: %" PRIu64 " in %d ms\n", result, int(duration.count()));
  REQUIRE(result == 49995000);
}

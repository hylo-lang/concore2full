#include "concore2full/spawn.h"
#include "concore2full/sync_execute.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <iterator>

static constexpr size_t size_threshold = 500;

template <typename T> inline T med3(T v1, T v2, T v3) {
  return v1 < v2 ? (v2 < v3 ? v2 : (v1 < v3 ? v3 : v1)) : (v3 < v2 ? v2 : (v1 < v3 ? v3 : v1));
}

template <std::random_access_iterator It> inline int median9(It it, int n) {
  assert(n >= 8);
  int stride = n / 8;
  int m1 = med3(*it, it[stride], it[stride * 2]);
  int m2 = med3(it[stride * 3], it[stride * 4], it[stride * 5]);
  int m3 = med3(it[stride * 6], it[stride * 7], it[n - 1]);
  return med3(m1, m2, m3);
}

template <std::random_access_iterator It> std::pair<It, It> sort_partition(It first, It last) {
  auto n = static_cast<int>(std::distance(first, last));
  auto pivot = median9(first, n);
  auto mid1 = std::partition(first, last, [=](const auto& val) { return val < pivot; });
  auto mid2 = std::partition(first, last, [=](const auto& val) { return !(pivot < val); });
  return {mid1, mid2};
}

template <std::random_access_iterator It> void my_concurrent_sort(It first, It last) {
  auto size = std::distance(first, last);
  if (size_t(size) < size_threshold) {
    // Use serial sort under a certain threshold.
    std::sort(first, last);
  } else {
    // Partition the data, such as elements [0, mid) < [mid] <= [mid+1, n).
    auto p = sort_partition(first, last);
    auto mid1 = p.first;
    auto mid2 = p.second;

    // Spawn work to sort the right-hand side.
    auto handle = concore2full::spawn([=] { my_concurrent_sort(mid2, last); });
    // Execute the sorting on the left side, on the current thread.
    my_concurrent_sort(first, mid1);
    // We are done when both sides are done.
    handle.await();
  }
}

TEST_CASE("concurrent sort example", "[examples]") {
  concore2full::sync_execute([] {
    std::vector<int> v;
    static constexpr int num_elem = 1'000;
    v.reserve(num_elem);
    for (int i = num_elem - 1; i >= 0; i--)
      v.push_back(i / 10);

    my_concurrent_sort(v.begin(), v.end());
    CHECK(std::is_sorted(v.begin(), v.end()));
  });
}

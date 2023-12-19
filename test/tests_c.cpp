#include <catch2/catch_test_macros.hpp>

extern "C" {
int test_basic_spawn();
}

TEST_CASE("spawn basic test", "[c]") { REQUIRE(test_basic_spawn()); }

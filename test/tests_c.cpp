#include <catch2/catch_test_macros.hpp>

extern "C" {
int test_basic_spawn();
int test_basic_bulk_spawn();
}

TEST_CASE("C: spawn basic test", "[c]") { REQUIRE(test_basic_spawn()); }
TEST_CASE("C: bulk_spawn basic test", "[c]") { REQUIRE(test_basic_bulk_spawn()); }

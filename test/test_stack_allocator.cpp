#include "concore2full/stack/simple_stack_allocator.h"
#include "concore2full/stack/stack_allocator.h"

#include <catch2/catch_test_macros.hpp>

#include <algorithm>
#include <memory>

using namespace concore2full;

TEST_CASE("simple_stack_allocator models stack_allocator", "[stack_allocator]") {
  REQUIRE(stack::stack_allocator<stack::simple_stack_allocator>);
}
TEST_CASE("std::allocator models stack_allocator", "[stack_allocator]") {
  REQUIRE(!stack::stack_allocator<std::allocator<int>>);
}

TEST_CASE("simple_stack_allocator can allocate memory", "[stack_allocator]") {
  // Arrange
  stack::simple_stack_allocator sut;

  // Act
  auto stack = sut.allocate();

  // Assert
  REQUIRE(stack.size > 0);
  REQUIRE(stack.sp != nullptr);

  // Destroy
  sut.deallocate(stack);
}

TEST_CASE("simple_stack_allocator can allocate stacks multiple times", "[stack_allocator]") {
  // Arrange
  stack::simple_stack_allocator sut;

  // Act
  auto stack1 = sut.allocate();
  auto stack2 = sut.allocate();

  // Assert
  REQUIRE(stack1.sp != stack2.sp);
  REQUIRE(stack1.sp != nullptr);
  REQUIRE(stack2.sp != nullptr);

  // Destroy
  sut.deallocate(stack1);
  sut.deallocate(stack2);
}

TEST_CASE("simple_stack_allocator allocates memory that can be filled", "[stack_allocator]") {
  // Arrange
  stack::simple_stack_allocator sut;
  constexpr uint8_t fill_value = 0xab;

  // Act: fill the memory with a special value
  auto stack = sut.allocate();
  auto end = reinterpret_cast<uint8_t*>(stack.sp);
  auto start = end - stack.size;
  std::fill(start, end, fill_value);

  // Assert
  auto it = std::find_if(start, end, [](uint8_t v) { return v != fill_value; });
  REQUIRE(it == end);

  // Destroy
  sut.deallocate(stack);
}

TEST_CASE("simple_stack_allocator allocates custom amount of memory", "[stack_allocator]") {
  // Arrange

  // Act
  stack::simple_stack_allocator sut(10);
  auto stack = sut.allocate();

  // Assert
  REQUIRE(stack.size == 10);

  // Destroy
  sut.deallocate(stack);
}

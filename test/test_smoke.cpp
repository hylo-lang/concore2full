#include <catch2/catch_test_macros.hpp>
#include <context_core_api.h>

struct stack_memory {
  stack_memory() {
    constexpr size_t stack_size = 1024;
    data_ = reinterpret_cast<uint8_t*>(malloc(stack_size));
    size_ = stack_size;
  }
  ~stack_memory() { free(data_); }

  void* end() const { return data_ + size_; }
  size_t size() const { return size_; }

private:
  uint8_t* data_;
  size_t size_;
};

TEST_CASE("smoke test for context-core-api", "[smoke]") {
  // Arrange
  stack_memory stack;
  int arr[3] = {0, 0, 0};
  struct t {
    static void context_fun(context_core_api_transfer_t param) {
      // for context_core_api_ontop_fcontext to work, we need to enter this context at least once.
      auto r = context_core_api_jump_fcontext(param.fctx, param.data);

      // Execute the code in the new context
      auto int_ptr = reinterpret_cast<int*>(r.data);
      *int_ptr = 1;

      // Jump back to the previous context
      context_core_api_jump_fcontext(r.fctx, int_ptr + 1);
    }
    static context_core_api_transfer_t ontop_fun(context_core_api_transfer_t param) {
      auto int_ptr = reinterpret_cast<int*>(param.data);
      *int_ptr = 2;
      return {param.fctx, int_ptr + 1};
    }
  };

  // Create a stackfull coroutine and get a context handle to its start
  auto ctx = context_core_api_make_fcontext(stack.end(), stack.size(), &t::context_fun);
  // Jump to the coroutine one time
  ctx = context_core_api_jump_fcontext(ctx, nullptr).fctx;
  // After we come back, execute a function on top of the coroutine
  auto r = context_core_api_ontop_fcontext(ctx, arr, &t::ontop_fun);

  // Assert
  REQUIRE(r.fctx != nullptr);
  REQUIRE(arr[0] == 2);       // first the function is called
  REQUIRE(arr[1] == 1);       // then the base context
  REQUIRE(arr[2] == 0);       // nobody will touch the third element
  REQUIRE(r.data == &arr[2]); // pointer to the third element will be returned
}

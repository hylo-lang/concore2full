#include "concore2full/spawn.h"
#include "concore2full/sync_execute.h"

#include <catch2/catch_test_macros.hpp>

#include <chrono>

using namespace std::chrono_literals;

template <typename Source> struct stop_token;
struct hierarchical_stop_source;

struct hierarchical_stop_source {
  hierarchical_stop_source() = default;
  explicit hierarchical_stop_source(stop_token<hierarchical_stop_source> parent);
  bool stop_requested() const noexcept {
    return stop_requested_.load(std::memory_order_acquire) ||
           (parent_ && parent_->stop_requested());
  }
  bool stop_possible() const noexcept { return true; }
  void request_stop() noexcept { stop_requested_.store(true, std::memory_order_release); }

  stop_token<hierarchical_stop_source> get_token() const noexcept;

private:
  std::atomic<bool> stop_requested_{false};
  const hierarchical_stop_source* parent_{nullptr};
};

template <typename Source> struct stop_token {
  explicit stop_token(const Source& source) : source_{source} {}
  bool stop_requested() const noexcept { return source_.stop_requested(); }
  bool stop_possible() const noexcept { return source_.stop_possible(); }
  void request_stop() noexcept { source_.request_stop(); }

  const Source& source() const noexcept { return source_; }

private:
  const Source& source_;
};

inline hierarchical_stop_source::hierarchical_stop_source(
    stop_token<hierarchical_stop_source> parent) {
  parent_ = &parent.source();
}

inline stop_token<hierarchical_stop_source> hierarchical_stop_source::get_token() const noexcept {
  return stop_token<hierarchical_stop_source>{*this};
}

void do_work(std::atomic<int>& counter, stop_token<hierarchical_stop_source> stop_token) {
  while (!stop_token.stop_requested()) {
    counter++;
    std::this_thread::sleep_for(1ms);

    if (counter.load(std::memory_order_relaxed) > 10'000) {
      FAIL("Too many iterations, expecting cancellation");
      break;
    }
  }
}

// We are implementing the following graph of tasks:
//    A -> B
//      B -> B.1
//    A -> C
// We do work in all the tasks, and we expect that all the tasks will be cancelled.
// As soon as we cancel the main task, we should cancel all the nested work.
TEST_CASE("hierarchical cancellation", "[examples]") {
  // Arrange
  std::atomic<int> counter1{0};
  std::atomic<int> counter2{0};
  std::atomic<int> counter3{0};
  std::atomic<int> counter4{0};

  // Start task A.
  hierarchical_stop_source main_stop_source;
  auto op = concore2full::spawn([&, stop_token = main_stop_source.get_token()] {
    // start a task with nested task (B)
    auto ss2 = hierarchical_stop_source{stop_token};
    auto op2 = concore2full::spawn([&, stop_token = ss2.get_token()] {
      // spawn some nested work (B.1)
      auto ss3 = hierarchical_stop_source{stop_token};
      auto op3 = concore2full::spawn([&, stop_token = ss3.get_token()] {
        // do some work, without spawning any nested task
        do_work(counter1, stop_token);
      });

      // do some work
      do_work(counter2, stop_token);

      op3.await();
    });

    // Start a simple task (C)
    auto ss4 = hierarchical_stop_source{stop_token};
    auto op4 = concore2full::spawn([&, stop_token = ss4.get_token()] {
      // do some work
      do_work(counter3, stop_token);
    });

    do_work(counter4, stop_token);

    // Await for all the spawned children
    op2.await();
    op4.await();
  });

  // Wait until we've managed to start all the tasks and incremented all the counters
  while (counter1.load(std::memory_order_acquire) == 0 ||
         counter2.load(std::memory_order_acquire) == 0 ||
         counter3.load(std::memory_order_acquire) == 0 ||
         counter4.load(std::memory_order_acquire) == 0)
    std::this_thread::sleep_for(1ms);
  // Now, request cancellation for the entire graph of tasks.
  main_stop_source.request_stop();
  op.await();

  // We should not have any errors because of iterations being too long
}

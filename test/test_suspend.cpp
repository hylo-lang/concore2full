#include "concore2full/global_thread_pool.h"
#include "concore2full/profiling.h"
#include "concore2full/suspend.h"
#include "concore2full/sync_execute.h"

#include <catch2/catch_test_macros.hpp>

#include <semaphore>

// TEST_CASE("suspend actually suspends the thread of execution", "[suspend]") {
//   concore2full::profiling::zone zone{CURRENT_LOCATION()};

//   // Arrange
//   concore2full::suspend_token* token_ptr{nullptr};
//   std::binary_semaphore token_created{0};
//   std::atomic<int> counter{0};
//   int before_suspend_value{-1};
//   int after_suspend_value{-1};
//   int before_notify_value{-1};
//   int after_notify_value{-1};

//   // Act
//   std::thread thread_to_suspend{[&] {
//     concore2full::profiling::zone zone{CURRENT_LOCATION_N("thread_to_suspend")};
//     concore2full::sync_execute([&] {
//       concore2full::profiling::zone z2{CURRENT_LOCATION()};
//       before_suspend_value = counter++;
//       concore2full::suspend_token token;
//       token_ptr = &token;
//       token_created.release();
//       concore2full::suspend(token);
//       after_suspend_value = counter++;
//     });
//   }};
//   std::thread notifying_thread{[&] {
//     concore2full::profiling::zone zone{CURRENT_LOCATION_N("notifying_thread")};
//     token_created.acquire();
//     before_notify_value = counter++;
//     token_ptr->notify();
//     after_notify_value = counter++;
//   }};
//   thread_to_suspend.join();
//   notifying_thread.join();

//   // Assert
//   REQUIRE(before_suspend_value == 0);
//   REQUIRE(before_notify_value == 1);
//   if (after_suspend_value == 2) {
//     REQUIRE(after_suspend_value == 2);
//     REQUIRE(after_notify_value == 3);
//   } else {
//     REQUIRE(after_suspend_value == 3);
//     REQUIRE(after_notify_value == 2);
//   }
//   REQUIRE(counter.load(std::memory_order_relaxed) == 4);
// }

TEST_CASE("notify is called before suspend", "[suspend]") {
  concore2full::profiling::zone zone{CURRENT_LOCATION()};

  // Arrange
  concore2full::suspend_token* token_ptr{nullptr};
  bool reched_after_suspend{false};
  std::binary_semaphore token_created{0};
  std::binary_semaphore notify_called{0};

  // Act
  std::thread thread_to_suspend{[&] {
    concore2full::profiling::zone zone{CURRENT_LOCATION_N("thread_to_suspend")};
    concore2full::sync_execute([&] {
      concore2full::profiling::zone z2{CURRENT_LOCATION()};
      // Create the token first
      concore2full::suspend_token token;
      token_ptr = &token;
      // Tell the other thread that the token is created, and wait for it to call notify.
      token_created.release();
      notify_called.acquire();
      // Now suspend, afer the notify was called
      concore2full::suspend(token);
      // We should be able to quickly exit the suspend call.
      reched_after_suspend = true;
    });
  }};
  std::thread notifying_thread{[&] {
    concore2full::profiling::zone zone{CURRENT_LOCATION_N("notifying_thread")};
    // Wait for token to be created.
    token_created.acquire();
    // Notify the token.
    token_ptr->notify();
    // Tell the other thread that the notify is done.
    notify_called.release();
  }};
  // Wait for both threads to finish.
  thread_to_suspend.join();
  notifying_thread.join();

  // Assert
  REQUIRE(reched_after_suspend);
}

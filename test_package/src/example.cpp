#include "concore2full/concore2full.h"

#include <chrono>
#include <iostream>
#include <semaphore>

using namespace std::chrono_literals;

int main() {
  bool called{false};
  std::binary_semaphore done{0};

  auto op{concore2full::spawn([&]() -> int {
    called = true;
    done.release();
    return 13;
  })};
  done.acquire();
  std::this_thread::sleep_for(1ms);
  concore2full::global_thread_pool().clear();
  auto res = op.await();
  std::cout << res << "\n";

  return 0;
}

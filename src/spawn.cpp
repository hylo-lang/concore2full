#include "concore2full/spawn.h"
#include "concore2full/profiling.h"

#include <chrono>

using namespace std::chrono_literals;

namespace concore2full::detail {

detail::continuation_t on_async_complete(spawn_data* data, detail::continuation_t c) {
  sync_state expected{sync_state::both_working};
  if (data->sync_state_.compare_exchange_strong(expected, sync_state::async_finished)) {
    // We are first to arrive at completion.
    // We won't need any thread switch, so we can safely exit.
    // Return the original continuation.
    return c;
  } else {
    // We are the last to arrive at completion, and we need a thread switch.

    // If the main thread is currently finishing, wait for it to finish.
    // We need the main thread to properly call `originator_start`.
    data->sync_state_.wait(sync_state::main_finishing, std::memory_order_acquire);

    // Finish the thread switch.
    return data->switch_data_.secondary_end();
  }
}

void on_main_complete(spawn_data* data) {
  sync_state expected{sync_state::both_working};
  if (data->sync_state_.compare_exchange_strong(expected, sync_state::main_finishing)) {
    // The main thread is first to finish; we need to start switching threads.
    auto c = detail::callcc([data](detail::continuation_t await_cc) -> detail::continuation_t {
      data->switch_data_.originator_start(await_cc);
      // We are done "finishing".
      data->sync_state_.store(sync_state::main_finished, std::memory_order_release);
      data->sync_state_.notify_one();
      // Ensure that we started the async work (and the continuation is set).
      data->async_started_.wait(false, std::memory_order_acquire);
      // Complete the thread switching.
      return data->switch_data_.originator_end();
    });
    (void)c;
  } else {
    // The async thread finished; we can continue directly, no need to switch threads.
  }
  // This point will be executed by the thread that finishes last.
}

void execute_spawn_task(spawn_data* data, int) noexcept {
#if USE_TRACY
  profiling::duplicate_zones_stack scoped_zones_stack{zones_};
#endif
  (void)detail::callcc([data](detail::continuation_t thread_cont) -> detail::continuation_t {
    // Assume there will be a thread switch and store required objects.
    data->switch_data_.secondary_start(thread_cont);
    // Signal the fact that we have started (and the continuation is properly stored).
    data->async_started_.store(true, std::memory_order_release);
    data->async_started_.notify_one();
    // Actually execute the given work.
    data->fptr_(data);
    // Complete the async processing.
    return on_async_complete(data, thread_cont);
  });
}

} // namespace concore2full::detail
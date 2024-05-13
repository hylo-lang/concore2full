#include "concore2full/detail/thread_suspension.h"
#include "concore2full/this_thread.h"

#include <utility>

void concore2full_store_thread_suspension(struct concore2full_thread_suspension* data,
                                          context_core_api_fcontext_t c) {
  data->continuation_ = c;
  data->thread_reclaimer_ = concore2full::this_thread::get_thread_reclaimer();
}
void concore2full_store_thread_suspension_release(struct concore2full_thread_suspension_sync* data,
                                                  context_core_api_fcontext_t c) {
  data->thread_reclaimer_ = concore2full::this_thread::get_thread_reclaimer();
  atomic_store_explicit(&data->continuation_, c, std::memory_order_release);
}

context_core_api_fcontext_t
concore2full_use_thread_suspension(struct concore2full_thread_suspension* data) {
  concore2full::this_thread::set_thread_reclaimer(
      reinterpret_cast<concore2full::thread_reclaimer*>(data->thread_reclaimer_));
  return data->continuation_;
}

context_core_api_fcontext_t
concore2full_use_thread_suspension_acquire(struct concore2full_thread_suspension_sync* data) {
  concore2full::this_thread::set_thread_reclaimer(
      reinterpret_cast<concore2full::thread_reclaimer*>(data->thread_reclaimer_));
  return atomic_load_explicit(&data->continuation_, std::memory_order_acquire);
}
context_core_api_fcontext_t
concore2full_use_thread_suspension_relaxed(struct concore2full_thread_suspension_sync* data) {
  concore2full::this_thread::set_thread_reclaimer(
      reinterpret_cast<concore2full::thread_reclaimer*>(data->thread_reclaimer_));
  return atomic_load_explicit(&data->continuation_, std::memory_order_relaxed);
}

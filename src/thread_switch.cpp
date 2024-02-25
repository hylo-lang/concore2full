#include "concore2full/c/thread_switch.h"
#include "concore2full/this_thread.h"

#include <utility>

void concore2full_store_thread_data_relaxed(struct concore2full_thread_data* data,
                                            context_core_api_fcontext_t c) {
  atomic_store_explicit(&data->context_, c, std::memory_order_relaxed);
  atomic_store_explicit(&data->thread_reclaimer_, concore2full::this_thread::get_thread_reclaimer(),
                        std::memory_order_relaxed);
}

void concore2full_store_thread_data_release(struct concore2full_thread_data* data,
                                            context_core_api_fcontext_t c) {
  atomic_store_explicit(&data->thread_reclaimer_, concore2full::this_thread::get_thread_reclaimer(),
                        std::memory_order_relaxed);
  atomic_store_explicit(&data->context_, c, std::memory_order_release);
}

context_core_api_fcontext_t
concore2full_exchange_thread_with(struct concore2full_thread_data* data) {
  concore2full::this_thread::set_thread_reclaimer(reinterpret_cast<concore2full::thread_reclaimer*>(
      atomic_load_explicit(&data->thread_reclaimer_, std::memory_order_relaxed)));
  // return atomic_exchange_explicit(&data->context_, nullptr, std::memory_order_relaxed);
  return atomic_load_explicit(&data->context_, std::memory_order_relaxed);
}

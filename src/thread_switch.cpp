#include "concore2full/c/thread_switch.h"
#include "concore2full/this_thread.h"

#include <utility>

void concore2full_store_thread_data(struct concore2full_thread_data* data,
                                    context_core_api_fcontext_t c) {
  data->context_ = c;
  data->thread_reclaimer_ = concore2full::this_thread::get_thread_reclaimer();
}

context_core_api_fcontext_t
concore2full_exchange_thread_with(struct concore2full_thread_data* data) {
  concore2full::this_thread::set_thread_reclaimer(
      reinterpret_cast<concore2full::thread_reclaimer*>(data->thread_reclaimer_));
  return std::exchange(data->context_, nullptr);
}

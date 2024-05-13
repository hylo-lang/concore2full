#include "concore2full/c/bulk_spawn.h"
#include "concore2full/detail/bulk_spawn_frame_base.h"

using concore2full::detail::bulk_spawn_frame_base;

extern "C" uint64_t concore2full_frame_size(int32_t count) {
  return bulk_spawn_frame_base::frame_size(count);
}

void concore2full_bulk_spawn(struct concore2full_bulk_spawn_frame* frame, int32_t count,
                             concore2full_bulk_spawn_function_t f) {
  bulk_spawn_frame_base::from_interface(frame)->spawn(count, f);
}

void concore2full_bulk_spawn2(struct concore2full_bulk_spawn_frame* frame, int32_t* count,
                              concore2full_bulk_spawn_function_t* f) {
  concore2full_bulk_spawn(frame, *count, *f);
}

void concore2full_bulk_await(struct concore2full_bulk_spawn_frame* frame) {
  bulk_spawn_frame_base::from_interface(frame)->await();
}

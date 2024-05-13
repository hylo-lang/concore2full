#include "concore2full/spawn.h"
#include "concore2full/detail/bulk_spawn_frame_base.h"
#include "concore2full/detail/spawn_frame_base.h"

namespace {

using concore2full::detail::bulk_spawn_frame_base;
using concore2full::detail::spawn_frame_base;

} // namespace

void concore2full_spawn(concore2full_spawn_frame* frame, concore2full_spawn_function_t f) {
  spawn_frame_base::from_interface(frame)->spawn(f);
}

void concore2full_await(concore2full_spawn_frame* frame) {
  spawn_frame_base::from_interface(frame)->await();
}

uint64_t concore2full_frame_size(int32_t count) { return bulk_spawn_frame_base::frame_size(count); }

void concore2full_bulk_spawn(struct concore2full_bulk_spawn_frame* frame, int32_t count,
                             concore2full_bulk_spawn_function_t f) {
  bulk_spawn_frame_base::from_interface(frame)->spawn(count, f);
}

void concore2full_bulk_await(struct concore2full_bulk_spawn_frame* frame) {
  bulk_spawn_frame_base::from_interface(frame)->await();
}

void concore2full_spawn2(concore2full_spawn_frame* frame, concore2full_spawn_function_t* f) {
  concore2full_spawn(frame, *f);
}

void concore2full_bulk_spawn2(struct concore2full_bulk_spawn_frame* frame, int32_t* count,
                              concore2full_bulk_spawn_function_t* f) {
  concore2full_bulk_spawn(frame, *count, *f);
}

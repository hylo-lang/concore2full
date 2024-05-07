#include "concore2full/spawn.h"

#include <chrono>
#include <cstring>

namespace {

using concore2full::detail::spawn_frame_base;

} // namespace

extern "C" void concore2full_spawn(concore2full_spawn_frame* frame,
                                   concore2full_spawn_function_t f) {
  spawn_frame_base::from_interface(frame)->spawn(f);
}

extern "C" void concore2full_await(concore2full_spawn_frame* frame) {
  spawn_frame_base::from_interface(frame)->await();
}

extern "C" void concore2full_spawn2(concore2full_spawn_frame* frame,
                                    concore2full_spawn_function_t* f) {
  concore2full_spawn(frame, *f);
}

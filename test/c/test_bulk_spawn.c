#include "concore2full/c/bulk_spawn.h"

#include <stdio.h>
#include <stdlib.h>

struct spawn_frame {
  int result_[10];
  int captures_;
  struct concore2full_bulk_spawn_frame base_;
};

static struct spawn_frame* alloc_frame(int count) {
  size_t size_base_frame = concore2full_frame_size(count);
  struct spawn_frame* frame = (struct spawn_frame*)malloc(
      sizeof(struct spawn_frame) - sizeof(struct concore2full_bulk_spawn_frame) + size_base_frame);
  return frame;
}

static void spawn_function(struct concore2full_bulk_spawn_frame* base_frame, uint64_t index) {
  char* p = (char*)base_frame;
  struct spawn_frame* frame = (struct spawn_frame*)(p - offsetof(struct spawn_frame, base_));
  printf("Hello, bulk of concurrent world, from worker %d!\n", (int)index);
  frame->result_[index] = 13 + frame->captures_;
}

int test_basic_bulk_spawn() {
  // Perform the spawn.
  struct spawn_frame* frame = alloc_frame(3);
  frame->captures_ = 11;
  concore2full_bulk_spawn(&frame->base_, 3, &spawn_function);
  // Do something else in on the main thread.
  printf("bulk main thread\n");
  // Await the result from the spawn.
  concore2full_bulk_await(&frame->base_);
  // Check the result.
  for (int i = 0; i < 3; ++i) {
    if (frame->result_[i] != 24)
      return 0;
  }
  free(frame);
  return 1;
}

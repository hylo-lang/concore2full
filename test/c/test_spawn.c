#include "concore2full/c/spawn.h"

#include <stdio.h>

struct spawn_frame {
  struct concore2full_spawn_frame base_;
  int result_;
  int captures_;
};

void spawn_function(struct concore2full_spawn_frame* base_frame) {
  struct spawn_frame* frame = (struct spawn_frame*)base_frame;
  printf("Hello, concurrent world!\n");
  frame->result_ = 13 + frame->captures_;
}

int test_basic_spawn() {
  // Perform the spawn.
  struct spawn_frame frame;
  frame.captures_ = 11;
  concore2full_spawn(&frame.base_, &spawn_function);
  // Do something else in on the main thread.
  printf("main thread\n");
  // Await the result from the spawn.
  concore2full_await(&frame.base_);
  // Check the result.
  return frame.result_ == 24;
}

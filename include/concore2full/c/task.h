#ifndef __CONCORE2FULL_TASK_H__
#define __CONCORE2FULL_TASK_H__

#ifdef __cplusplus
extern "C" {
#endif

struct concore2full_task;

//! Type of a function that can be executed as a task.
typedef void (*concore2full_task_function_t)(struct concore2full_task* task, int worker_index);

//! A task that can be executed.
struct concore2full_task {

  //! The function to be called to execute the task.
  concore2full_task_function_t task_function_;

  //! Pointer to the next element in the list of task; implementation details.
  struct concore2full_task* next_;
  //! Pointer to the `next_` field of the previous element in the list of tasks; implementation
  //! details.
  struct concore2full_task** prev_link_;
  //! The worker data for the task; implementation details.
  void* worker_data_;
};

#ifdef __cplusplus
}
#endif

#endif

#pragma once

#include <atomic>

namespace concore2full {
class thread_reclaimer;
}

namespace concore2full::this_thread {

/**
 * @brief Get the `thread_reclaimer` object associated with the current thread.
 * @return The `thread_reclaimer` object on this thread.
 *
 * The returned object should be owned by the thread pool that operates on this thread. It can be
 * used to tell the thread pool that someone needs to perform a thread switch with this thread.
 */
thread_reclaimer* get_thread_reclaimer();

/**
 * @brief Set the `thread_reclaimer` object for this thread.
 * @param new_reclaimer The new reclaimer object we want to set.
 *
 * The object set to the current thread should be owned by the thread pool that operates on this
 * thread. It can be used to tell the thread pool that someone needs to perform a thread switch with
 * this thread.
 */
void set_thread_reclaimer(thread_reclaimer* new_reclaimer);

/**
 * @brief Checkpoint for performing thread inversions.
 *
 * This is regularly called by the thread pool that owns the current thread to check if there are
 * any thread switches that involve the current thread. If there are, this function will also
 * perform the thread switch and may return on a different thread.
 *
 * Even if the thread changes, the reclaimer object that was set on the previous thread shall also
 * be set on the new thread (the thread reclaimer will also be exchanged). This is needed as the
 * pool that operates on the current thread doesn't change.
 */
void inversion_checkpoint();

} // namespace concore2full::this_thread

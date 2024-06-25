#pragma once

#include <atomic>

namespace concore2full::this_thread {

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

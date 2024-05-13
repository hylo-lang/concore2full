#ifndef __CONCORE2FULL_ATOMIC_WRAPPER_H__
#define __CONCORE2FULL_ATOMIC_WRAPPER_H__

#ifdef __cplusplus
#include <atomic>
#define CONCORE2FULL_ATOMIC(x) std::atomic<x>
#else
#define CONCORE2FULL_ATOMIC(x) volatile x
#endif

#endif

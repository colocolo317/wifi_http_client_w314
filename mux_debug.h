/*
 * mux_debug.h
 *
 *  Created on: 2024/5/8
 *      Author: ch.wang
 */

#ifndef MUX_DEBUG_H_
#define MUX_DEBUG_H_

#include "cmsis_os2.h"


#if defined(AMPAK_OS_DEBUG_LOG)
#define MUX_LOG(...)                                \
  {                                                   \
    osMutexAcquire(debug_prints_mutex, 0xFFFFFFFFUL); \
    printf(__VA_ARGS__);                              \
    osMutexRelease(debug_prints_mutex);               \
  }
#else
#define MUX_LOG(...)
#endif

extern osMutexId_t debug_prints_mutex;
osMutexId_t mux_debug_init(void);

#endif /* MUX_DEBUG_H_ */

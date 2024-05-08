/*
 * mux_debug.h
 *
 *  Created on: 2024年5月8日
 *      Author: ch.wang
 */

#ifndef MUX_DEBUG_H_
#define MUX_DEBUG_H_


#if defined(AMPAK_OS_DEBUG_LOG)
#define LOG_PRINT(...)                                \
  {                                                   \
    osMutexAcquire(debug_prints_mutex, 0xFFFFFFFFUL); \
    printf(__VA_ARGS__);                              \
    osMutexRelease(debug_prints_mutex);               \
  }
#elif defined(DEBUGOUT)
#define LOG_PRINT(...) DEBUGOUT(__VA_ARGS__)
#else
#define LOG_PRINT(...)
#endif


#endif /* MUX_DEBUG_H_ */

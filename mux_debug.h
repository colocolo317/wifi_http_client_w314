/*
 * mux_debug.h
 *
 *  Created on: 2024/5/8
 *      Author: ch.wang
 */

#ifndef MUX_DEBUG_H_
#define MUX_DEBUG_H_

#include "cmsis_os2.h"
#include "stdio.h"
#include <stdarg.h>

#define AMPAK_HTTP_DEBUG_LOG 1
#define AMPAK_RINGBUFF_DEBUG_LOG 1

#if defined(AMPAK_OS_DEBUG_LOG)
#define MUX_LOG(...) _MUX_LOG(__VA_ARGS__)
#else
#define MUX_LOG(...)
#endif
#if defined(AMPAK_HTTP_DEBUG_LOG)
#define http_debug_log(...) _http_debug_log(__VA_ARGS__)
#else
#define http_debug_log(...)
#endif
#if defined(AMPAK_RINGBUFF_DEBUG_LOG)
#define ringBuffer_debug(...) _ringBuffer_debug(__VA_ARGS__)
#else
#define ringBuffer_debug(...)
#endif


extern osMutexId_t debug_prints_mutex;
osMutexId_t mux_debug_init(void);

void _MUX_LOG(char* format, ...);
void _http_debug_log(char* format, ...);
void _ringBuffer_debug(char* format, ...);

#endif /* MUX_DEBUG_H_ */

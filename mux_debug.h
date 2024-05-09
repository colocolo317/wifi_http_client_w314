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

extern osMutexId_t debug_prints_mutex;
osMutexId_t mux_debug_init(void);

void MUX_LOG(char* format, ...);
void http_debug_log(char* format, ...);

#endif /* MUX_DEBUG_H_ */

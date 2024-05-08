/*
 * mux_debug.c
 *
 *  Created on: 2024/5/8
 *      Author: ch.wang
 */
#include "mux_debug.h"

osMutexId_t debug_prints_mutex = NULL;

osMutexId_t mux_debug_init(void)
{
  debug_prints_mutex = osMutexNew(NULL);
  return debug_prints_mutex;
}


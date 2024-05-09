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
  printf("\r\nmux debug init ok\r\n");
  return debug_prints_mutex;
}

#if defined(AMPAK_OS_DEBUG_LOG)
inline void MUX_LOG(char* format, ...)
{
  if(osMutexAcquire(debug_prints_mutex, 200) == osOK)
  {
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    osMutexRelease(debug_prints_mutex);
  }
  else
  {
    printf("%s", __func__ );
  }

}
#else
inline void MUX_LOG(char* format, ...)
{}
#endif

#if 0
inline void http_debug_log(char* format, ...)
{
  va_list args;
  va_start(args, format);
  vprintf(format, args);
  va_end(args);
}
#else
inline void http_debug_log(char* format, ...)
{}
#endif


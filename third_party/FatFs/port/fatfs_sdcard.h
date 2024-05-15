/*
 * fatfs_sdcard.h
 *
 *  Created on: 2024/3/19/
 *      Author: ch.wang
 */

#ifndef SDCARD_H_
#define SDCARD_H_
#include "ff.h"

#define MAX_READ_BUF_LEN 4096
#define SDCARD_BUF_SIZE 8192
#define FASTCLKTIME(a) ((a) * 32 / 180)
/**
 * FIXME: remove SL_SI91X_DMA=1 Macro cause SD card blocked
 */

#ifndef BIT
#define BIT(a) ((uint32_t)1U << a)
#endif

// Enumeration for states in application
typedef enum sdcard_state_e {
  SDCARD_INIT_STATE = 0,
  SDCARD_MOUNT_STATE = 1,
  SDCARD_CHANGE_DIR_STATE = 2,
  SDCARD_FILE_OPEN_TO_WRITE_STATE = 3,
  SDCARD_FILE_OPEN_TO_READ_STATE = 4,
  SDCARD_FILE_WRITE_STATE = 5,
  SDCARD_FILE_READ_STATE = 6,
  SDCARD_FILE_CLOSE_WRITE_STATE = 7,
  SDCARD_FILE_CLOSE_READ_STATE = 8,
  SDCARD_UNMOUNT_STATE = 9,
  SDCARD_DEINIT_STATE = 10,
//  SDCARD__STATE = ,
} sdcard_state_t;


void sdcard_task(void const *argument);

FIL* sdcard_get_wfile(void);
void dmesg(FRESULT fres);
void sdcard_status_print(const char* msg, FRESULT fres);
FRESULT sdcard_write(const void* data, const unsigned int len);
FRESULT sdcard_read(const void* data, const unsigned int len);

void ls(char *path);
void sdcard_set_event(uint32_t event_num);
void sdcard_clear_event(uint32_t event_num);

#endif /* SDCARD_H_ */

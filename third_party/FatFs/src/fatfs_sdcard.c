/*=======================================================================*/
//  ! INCLUDES
/*=======================================================================*/
#include "cmsis_os2.h"
#include <stdio.h>
#include <string.h>

#include "rsi_common_apis.h"
#include "rsi_debug.h"
#include "FreeRTOS.h"
//! FatFS
#include "fatfs.h"
#include "ff.h"
#include "fatfs_sdcard.h"
#include "ring_buff.h"

/*******************************************************************************
 **********************  Local Function prototypes   ***************************
 ******************************************************************************/
osThreadId_t SDinfoHandle;
osThreadId_t SDManagerHandle;
osMutexId_t SDCardMutexHandle;

extern osSemaphoreId_t sdcard_thread_sem;

FATFS FatFs;   // FATFS handle
FRESULT fres;  // Common result code
FILINFO fno;    // Structure holds information
FATFS *getFreeFs;     // Read information
static FIL file_write; // file_write
static FIL file_read;
DIR dir;        // Directory object structure
DWORD free_clusters;  // Free Clusters
DWORD free_sectors;   // Free Sectors
DWORD total_sectors;  // Total Sectors

extern char USERPath[4];   /* USER logical drive path */

static uint32_t bytesRead;
static uint32_t bytesWrote;

//extern StreamBufferHandle_t stream_buf_gspi;
//extern osMutexId_t stream_buf_mutex;
extern RingBuffer* ring_buff_p;

static uint32_t sdcard_event_map=0;
//static uint8_t sdcard_buf[SDCARD_BUF_SIZE];

void sdcard_set_event(uint32_t event_num)
{
  sdcard_event_map |= BIT(event_num);
  osSemaphoreRelease(sdcard_thread_sem);
  return;
}

void sdcard_clear_event(uint32_t event_num)
{
  sdcard_event_map &= ~BIT(event_num);
  return;
}

int32_t sdcard_get_event(void)
{
  uint32_t ix;
  for (ix = 0; ix < 32; ix++) {
    if (sdcard_event_map & (1 << ix)) {
      return ix;
    }
  }
  return (-1);
}


void sdcard_task(void const *argument)
{
  UNUSED_PARAMETER(argument);
  int32_t event_id = 0;
  while(1)
  {
      event_id = sdcard_get_event();
      //LOG_PRINT("sdcard get event %d \r\n", event_id);
      if (event_id == -1) {
        //LOG_PRINT("sdcard hold with no event\r\n");
        osSemaphoreAcquire(sdcard_thread_sem, osWaitForever);
        // if events are not received loop will be continued.
        continue;
      }

      switch (event_id)
      {
        case SDCARD_INIT_STATE:
          {
            LOG_PRINT("SDCARD_INIT_STATE\r\n");
            sdcard_clear_event(SDCARD_INIT_STATE);
            MX_FATFS_Init();
            sdcard_set_event(SDCARD_MOUNT_STATE);
          }break;
        case SDCARD_MOUNT_STATE:
          {
            LOG_PRINT("SDCARD_MOUNT_STATE\r\n");
            sdcard_clear_event(SDCARD_MOUNT_STATE);
            /* GSPI init here*/
            fres = f_mount(&FatFs, (const TCHAR*) USERPath, 1); // 1 -> Mount now
            sdcard_status_print("mount",fres);
            sdcard_set_event(SDCARD_CHANGE_DIR_STATE);
          }break;
        case SDCARD_CHANGE_DIR_STATE:
          {
            LOG_PRINT("SDCARD_CHANGE_DIR_STATE\r\n");
            sdcard_clear_event(SDCARD_CHANGE_DIR_STATE);
            fres = f_mkdir("DEMO");
            sdcard_status_print("mkdir DEMO",fres);
            fres = f_chdir("DEMO");
            sdcard_status_print("chdir DEMO",fres);
            fres = f_mkdir("MEDIA");
            sdcard_status_print("mkdir MEDIA",fres);
            fres = f_chdir("MEDIA");
            sdcard_status_print("chdir MEDIA",fres);
            sdcard_set_event(SDCARD_FILE_OPEN_TO_WRITE_STATE);
          }break;
        case SDCARD_FILE_OPEN_TO_WRITE_STATE:
          {
            LOG_PRINT("SDCARD_FILE_OPEN_TO_WRITE_STATE\r\n");
            sdcard_clear_event(SDCARD_FILE_OPEN_TO_WRITE_STATE);
#if AMPAK_USE_SDCARD_WRITE
            fres = f_unlink("download.bin");
            sdcard_status_print("Delete file",fres);
            // Write File
            fres = f_open(&file_write, "download.bin", FA_WRITE | FA_OPEN_ALWAYS );
            sdcard_status_print("open file to write",fres);
#endif
            //sdcard_set_event(); //wait to
          }break;
        case SDCARD_FILE_OPEN_TO_READ_STATE:
          {
            LOG_PRINT("SDCARD_FILE_OPEN_TO_READ_STATE\r\n");
            sdcard_clear_event(SDCARD_FILE_OPEN_TO_READ_STATE);
            fres = f_open(&file_read, "download.bin", FA_READ);
            sdcard_status_print("open file to read",fres);
          }break;
        case SDCARD_FILE_WRITE_STATE:
          {
            //LOG_PRINT("SDCARD_FILE_WRITE_STATE\r\n");
            //LOG_PRINT("h %u, t %u, l %lu\r\n", ring_buff_p->head, ring_buff_p->tail, ring_buff_p->data_len[ring_buff_p->head]);
#if 0
            if(!ringBuffer_IsOne(ring_buff_p))
            {
              LOG_PRINT("Write: h %u, t %u, hl %lu, tl %lu\r\n", ring_buff_p->head, ring_buff_p->tail, ring_buff_p->data_len[ring_buff_p->head], ring_buff_p->data_len[ring_buff_p->tail]);
              fres = f_write(&file_write, ring_buff_p->buffer[ring_buff_p->tail], ring_buff_p->data_len[ring_buff_p->tail], &bytesWrote);

              if(fres != FR_OK)
              {
                LOG_PRINT("bytesWrote: %u\r\n",bytesWrote);
                sdcard_status_print("f_write sdcard", fres);
              }
              ringBuffer_reduce(ring_buff_p);

              LOG_PRINT("reduce ring buffer\r\n");
              LOG_PRINT("h %u, t %u, hl %lu, tl %lu\r\n", ring_buff_p->head, ring_buff_p->tail, ring_buff_p->data_len[ring_buff_p->head], ring_buff_p->data_len[ring_buff_p->tail]);
            }
#endif
            sdcard_clear_event(SDCARD_FILE_WRITE_STATE);

          }break;
        case SDCARD_FILE_READ_STATE:
          {
            LOG_PRINT("SDCARD_FILE_READ_STATE\r\n");
            sdcard_clear_event(SDCARD_FILE_READ_STATE);
          }break;
        case SDCARD_FILE_CLOSE_STATE:
          {
            LOG_PRINT("SDCARD_FILE_CLOSE_STATE\r\n");

            while(!ringBuffer_IsEmpty(ring_buff_p))
            {
#if AMPAK_USE_SDCARD_WRITE
              fres = f_write(&file_write, ring_buff_p->buffer[ring_buff_p->tail], ring_buff_p->data_len[ring_buff_p->tail], &bytesWrote);

              if(fres != FR_OK)
              {
                LOG_PRINT("bytesWrote: %u\r\n",bytesWrote);
                sdcard_status_print("f_write sdcard", fres);
              }
#endif
              if(!ringBuffer_IsOne(ring_buff_p))
              {
                  ringBuffer_reduce(ring_buff_p);
              }
              else
              {
                ring_buff_p->data_len[ring_buff_p->tail] = 0;
              }
            }
#if AMPAK_USE_SDCARD_WRITE
            fres=f_close(&file_write);
            sdcard_status_print("f_close", fres);
#endif
            sdcard_clear_event(SDCARD_FILE_CLOSE_STATE);
            //sdcard_set_event(SDCARD_UNMOUNT_STATE);
          }break;
        case SDCARD_FILE_CLOSE_READ_STATE:
          {

          }break;
        case SDCARD_UNMOUNT_STATE:
          {
            LOG_PRINT("SDCARD_UNMOUNT_STATE\r\n");
            ls("");
            fres=f_unmount((const TCHAR*) USERPath);
            sdcard_status_print("f_unmount((const TCHAR*) USERPath)", fres);
            sdcard_clear_event(SDCARD_UNMOUNT_STATE);
            sdcard_set_event(SDCARD_DEINIT_STATE);

          }break;
        case SDCARD_DEINIT_STATE:
          {
            LOG_PRINT("SDCARD_DEINIT_STATE\r\n");
            sdcard_clear_event(SDCARD_DEINIT_STATE);
            fres= (FRESULT) FATFS_UnLinkDriver(USERPath);
            sdcard_status_print("FATFS_UnLinkDriver", fres);

          }break;
        default:
          {
            LOG_PRINT("SD card UNKNOWN state: %lu\r\n",(uint32_t)event_id);
            sdcard_clear_event((uint32_t)event_id);
          }break;
      }

  }
}


FRESULT sdcard_write(const void* data, const unsigned int len)
{
  return f_write(&file_write, data, len, &bytesWrote);
}

FRESULT sdcard_read(const void* data, const unsigned int len)
{
  return f_read(&file_read, data, len, &bytesRead);
}

FIL* sdcard_get_wfile(){
  return &file_write;
}

void sdcard_status_print(const char* msg, FRESULT fres)
{
  LOG_PRINT("[FatFS]: %s : ", msg);
  dmesg(fres);
}

void dmesg(FRESULT fres) {
  switch (fres) {
  case FR_OK:
    LOG_PRINT("Succeeded \r\n");
    break;
  case FR_DISK_ERR:
    LOG_PRINT("A hard error occurred in the low level disk I/O layer \r\n");
    break;
  case FR_INT_ERR:
    LOG_PRINT("Assertion failed \r\n");
    break;
  case FR_NOT_READY:
    LOG_PRINT("The physical drive cannot work\r\n");
    break;
  case FR_NO_FILE:
    LOG_PRINT("Could not find the file \r\n");
    break;
  case FR_NO_PATH:
    LOG_PRINT("Could not find the path \r\n");
    break;
  case FR_INVALID_NAME:
    LOG_PRINT("The path name format is invalid \r\n");
    break;
  case FR_DENIED:
    LOG_PRINT("Access denied due to prohibited access or directory full \r\n");
    break;
  case FR_EXIST:
    LOG_PRINT("Exist or access denied due to prohibited access \r\n");
    break;
  case FR_INVALID_OBJECT:
    LOG_PRINT("The file/directory object is invalid \r\n");
    break;
  case FR_WRITE_PROTECTED:
    LOG_PRINT("The physical drive is write protected \r\n");
    break;
  case FR_INVALID_DRIVE:
    LOG_PRINT("The logical drive number is invalid \r\n");
    break;
  case FR_NOT_ENABLED:
    LOG_PRINT("The volume has no work area\r\n");
    break;
  case FR_NO_FILESYSTEM:
    LOG_PRINT("There is no valid FAT volume\r\n");
    break;
  case FR_MKFS_ABORTED:
    LOG_PRINT("The f_mkfs() aborted due to any parameter error \r\n");
    break;
  case FR_TIMEOUT:
    LOG_PRINT(
        "Could not get a grant to access the volume within defined period \r\n");
    break;
  case FR_LOCKED:
    LOG_PRINT(
        "The operation is rejected according to the file sharing policy \r\n");
    break;
  case FR_NOT_ENOUGH_CORE:
    LOG_PRINT("LFN working buffer could not be allocated \r\n");
    break;
  case FR_TOO_MANY_OPEN_FILES:
    LOG_PRINT("Number of open files > _FS_SHARE \r\n");
    break;
  case FR_INVALID_PARAMETER:
    LOG_PRINT("Given parameter is invalid \r\n");
    break;
  default:
    LOG_PRINT("An error occured. (%d)\r\n", fres);
  }

}

void ls(char *path) {

  LOG_PRINT("\r\nFiles/Folder List:\r\n");
  fres = f_opendir(&dir, path);

  if (fres == FR_OK) {
    while (1) {

      fres = f_readdir(&dir, &fno);

      if ((fres != FR_OK) || (fno.fname[0] == 0)) {
        break;
      }
      LOG_PRINT(" %c%c%c%c%c %u-%02u-%02u, %02u:%02u %10d %s/%s\r\n",
          ((fno.fattrib & AM_DIR) ? 'D' : '-'),
          ((fno.fattrib & AM_RDO) ? 'R' : '-'),
          ((fno.fattrib & AM_SYS) ? 'S' : '-'),
          ((fno.fattrib & AM_HID) ? 'H' : '-'),
          ((fno.fattrib & AM_ARC) ? 'A' : '-'),
          ((fno.fdate >> 9) + 1980), (fno.fdate >> 5 & 15),
          (fno.fdate & 31), (fno.ftime >> 11), (fno.ftime >> 5 & 63),
          (int) fno.fsize, path, fno.fname);
    }
  }
  LOG_PRINT("\r\n");
}

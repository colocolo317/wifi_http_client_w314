/*
 * gspi_control.c
 *
 *  Created on: 2024/5/8
 *      Author: ch.wang
 */
#include "cmsis_os2.h"
#include "sl_si91x_gspi.h"
#include "sl_si91x_gspi_common_config.h"
#include "gspi_util.h"
#include "rsi_debug.h"
#include "mux_debug.h"
#include "ring_buff.h"

/*******************************************************************************
 ***************************  Defines / Macros  ********************************
 ******************************************************************************/
#define GSPI_BUFFER_SIZE             1024      // Size of buffer
#define GSPI_INTF_PLL_CLK            180000000 // Intf pll clock frequency
#define GSPI_INTF_PLL_REF_CLK        40000000  // Intf pll reference clock frequency
#define GSPI_SOC_PLL_CLK             20000000  // Soc pll clock frequency
#define GSPI_SOC_PLL_REF_CLK         40000000  // Soc pll reference clock frequency
#define GSPI_INTF_PLL_500_CTRL_VALUE 0xD900    // Intf pll control value
#define GSPI_SOC_PLL_MM_COUNT_LIMIT  0xA4      // Soc pll count limit
#define GSPI_DVISION_FACTOR          0         // Division factor
#define GSPI_SWAP_READ_DATA          1         // true to enable and false to disable swap read
#define GSPI_SWAP_WRITE_DATA         0         // true to enable and false to disable swap write
#define GSPI_BITRATE                 10000000  // Bitrate for setting the clock division factor
#define GSPI_BIT_WIDTH               8         // Default Bit width
#define GSPI_MAX_BIT_WIDTH           16        // Maximum Bit width

/**
 * GLOBAL VARIABLES
 */
extern osSemaphoreId_t gspi_thread_sem;

/*******************************************************************************
 *************************** LOCAL VARIABLES   *******************************
 ******************************************************************************/
uint16_t gspi_data_out[GSPI_BUFFER_SIZE];
uint16_t gspi_data_in[GSPI_BUFFER_SIZE];
static uint16_t gspi_division_factor       = 1;
static sl_gspi_handle_t gspi_driver_handle = NULL;
static boolean_t transfer_complete  = false;

static osSemaphoreId_t gspi_transfer_complete_sem;
/*******************************************************************************
 **********************  Local Function prototypes   ***************************
 ******************************************************************************/
static void callback_event(uint32_t event);
static sl_status_t gspi_transfer_test(void);
/*******************************************************************************
 **************************   GLOBAL FUNCTIONS   *******************************
 ******************************************************************************/
void callback_event(uint32_t event)
{
  switch (event)
  {
    case SL_GSPI_TRANSFER_COMPLETE:
      MUX_LOG("GSPI transfer_complete \r\n");
      transfer_complete = true;
      osSemaphoreRelease(gspi_transfer_complete_sem);
      break;
    case SL_GSPI_DATA_LOST:
      break;
    case SL_GSPI_MODE_FAULT:
      break;
  }
}
/*******************************************************************************
 * GSPI example initialization function
 ******************************************************************************/
void gspi_init(void)
{
  sl_status_t status;
#if AMPAK_GSPI_MODIFY_CLOCK
  sl_gspi_clock_config_t clock_config;
#endif
  sl_gspi_version_t version;
  sl_gspi_status_t gspi_status;
  sl_gspi_control_config_t config;
  config.bit_width         = GSPI_BIT_WIDTH;
  config.bitrate           = GSPI_BITRATE;
  config.clock_mode        = SL_GSPI_MODE_0;
  config.slave_select_mode = SL_GSPI_MASTER_HW_OUTPUT;
  config.swap_read         = GSPI_SWAP_READ_DATA;
  config.swap_write        = GSPI_SWAP_WRITE_DATA;
  do {
      // Version information of GSPI driver
      version = sl_si91x_gspi_get_version();
      MUX_LOG("GSPI version is fetched successfully \r\n");
      MUX_LOG("API version is %d.%d.%d\n", version.release, version.major, version.minor);
      // Filling up the structure with the default clock parameters

#if AMPAK_GSPI_MODIFY_CLOCK
      status = init_clock_configuration_structure(&clock_config);
      if (status != SL_STATUS_OK) {
        MUX_LOG("init_clock_configuration_structure: Error Code : %lu \r\n", status);
        break;
      }
      // Configuration of clock with the default clock parameters
      status = sl_si91x_gspi_configure_clock(&clock_config);
      if (status != SL_STATUS_OK) {
        MUX_LOG("sl_si91x_gspi_clock_configuration: Error Code : %lu \r\n", status);
        break;
      }
      MUX_LOG("Clock configuration is successful \r\n");
      // Pass the address of void pointer, it will be updated with the address
      // of GSPI instance which can be used in other APIs.
#endif
      status = sl_si91x_gspi_init(SL_GSPI_MASTER, &gspi_driver_handle);
      if (status != SL_STATUS_OK) {
        MUX_LOG("sl_si91x_gspi_init: Error Code : %lu \r\n", status);
        break;
      }
      MUX_LOG("GSPI initialization is successful \r\n");
      // Fetching the status of GSPI i.e., busy, data lost and mode fault
      gspi_status = sl_si91x_gspi_get_status(gspi_driver_handle);
      MUX_LOG("GSPI status is fetched successfully \r\n");
      MUX_LOG("Busy: %d\r\n", gspi_status.busy);
      MUX_LOG("Data_Lost: %d\r\n", gspi_status.data_lost);
      MUX_LOG("Mode_Fault: %d\r\n", gspi_status.mode_fault);
      //Configuration of all other parameters that are required by GSPI
      // gspi_configuration structure is from sl_si91x_gspi_init.h file.
      // The user can modify this structure with the configuration of
      // his choice by filling this structure.
      status = sl_si91x_gspi_set_configuration(gspi_driver_handle, &config);
      if (status != SL_STATUS_OK) {
        MUX_LOG("sl_si91x_gspi_control: Error Code : %lu \r\n", status);
        break;
      }
      MUX_LOG("GSPI configuration is successful \r\n");
      // Register user callback function
      status = sl_si91x_gspi_register_event_callback(gspi_driver_handle, callback_event);
      if (status != SL_STATUS_OK) {
        MUX_LOG("sl_si91x_gspi_register_event_callback: Error Code : %lu \r\n", status);
        break;
      }
      MUX_LOG("GSPI user event callback registered successfully \r\n");
      // Fetching and printing the current clock division factor
      MUX_LOG("Current Clock division factor is %lu \r\n", sl_si91x_gspi_get_clock_division_factor(gspi_driver_handle));
      // Fetching and printing the current frame length
      MUX_LOG("Current Frame Length is %lu \r\n", sl_si91x_gspi_get_frame_length());
      if (sl_si91x_gspi_get_frame_length() > GSPI_BIT_WIDTH) {
        gspi_division_factor = sizeof(gspi_data_out[0]);
      }

      gspi_transfer_complete_sem = osSemaphoreNew(1, 1, NULL);
      if(gspi_transfer_complete_sem == NULL)
      {
          MUX_LOG("Failed to new semaphore gspi_transfer_complete_sem\r\n");
          break;
      }
  }while(false);

}

void gspi_task(void* arguments)
{
  sl_status_t status;
  while(1)
  {
    osSemaphoreAcquire(gspi_thread_sem, osWaitForever);
    if(ringBuffer_IsOne(pRingBuff) != true)
    {
      transfer_complete = false;
      osSemaphoreAcquire(gspi_transfer_complete_sem, osWaitForever);
      status = sl_si91x_gspi_set_slave_number(GSPI_SLAVE_0);
      status = sl_si91x_gspi_transfer_data(gspi_driver_handle,
                                           pRingBuff->buffer[pRingBuff->tail],
                                           gspi_data_in,
                                           pRingBuff->data_len[pRingBuff->tail]);
      if (status != SL_STATUS_OK)
      {
        // If it fails to execute the API, it will not execute rest of the things
        MUX_LOG("sl_si91x_gspi_transfer_data: Error Code : %lu \r\n", status);

      }
      else{
        MUX_LOG("w");
        if(ringBuffer_reduce(pRingBuff) == false)
        { MUX_LOG("[GSPI] Failed to reduce ring buffer\r\n");}
      }
    }
  }
}

sl_status_t gspi_transfer_test(void)
{
  sl_status_t status;
  status = sl_si91x_gspi_set_slave_number(GSPI_SLAVE_0);
  status = sl_si91x_gspi_transfer_data(gspi_driver_handle,
                                       gspi_data_out,
                                       gspi_data_in,
                                       sizeof(gspi_data_out) / gspi_division_factor);
  if (status != SL_STATUS_OK)
  {
    // If it fails to execute the API, it will not execute rest of the things
    MUX_LOG("sl_si91x_gspi_transfer_data: Error Code : %lu \r\n", status);
    return status;
  }
  MUX_LOG("w");
  return status;
}

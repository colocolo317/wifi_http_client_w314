/***************************************************************************/ /**
 * @file
 * @brief HTTP Client Application
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************/
#include "sl_board_configuration.h"
#include "cmsis_os2.h"
#include "FreeRTOS.h"
#include "sl_wifi.h"
#include "sl_net.h"
#include "sl_http_client.h"
#include <string.h>
#include "gspi_util.h"
#include "mux_debug.h"
#include "ring_buff.h"

//! Include index html page
#include "index.html.h"
/******************************************************
 *                      Macros
 ******************************************************/
#define CLEAN_HTTP_CLIENT_IF_FAILED(status, client_handle, is_sync) \
  {                                                                 \
    if (status != SL_STATUS_OK) {                                   \
      sl_http_client_deinit(client_handle);                         \
      return ((is_sync == 0) ? status : callback_status);           \
    }                                                               \
  }

/******************************************************
 *                    Constants
 ******************************************************/
//! HTTP Client Configurations
#define HTTP_CLIENT_USERNAME "admin"
#define HTTP_CLIENT_PASSWORD "admin"

#define IP_VERSION   SL_IPV4
#define HTTP_VERSION SL_HTTP_V_1_1

//! Set 1 - Enable and 0 - Disable
#define HTTPS_ENABLE 0

//! Set 1 - Enable and 0 - Disable
#define EXTENDED_HEADER_ENABLE 0

#if HTTPS_ENABLE
//! Set TLS version
#define TLS_VERSION SL_TLS_V_1_2

#define CERTIFICATE_INDEX SL_HTTPS_CLIENT_CERTIFICATE_INDEX_1

//! Include SSL CA certificate
#include "cacert.pem.h"

//! Load certificate to device flash :
//! Certificate could be loaded once and need not be loaded for every boot up
#define LOAD_CERTIFICATE 1
#endif

//! HTTP request configurations
//! Set HTTP Server IP Address
#define HTTP_SERVER_IP "10.10.28.166"

//! Server port number
#if HTTPS_ENABLE
//! Set default HTTPS port
#define HTTP_PORT 443
#else
//! Set default HTTP port
#define HTTP_PORT 80
#endif

//! HTTP resource name
#define HTTP_URL "/dev/upload/sample.bin"

//! HTTP post data
#define HTTP_DATA "employee_name=MR.REDDY&employee_id=RSXYZ123&designation=Engineer&company=SILABS&location=Hyderabad"

#if EXTENDED_HEADER_ENABLE
//! Extended headers
#define KEY1 "Content-Type"
#define VAL1 "text/html; charset=utf-8"

#define KEY2 "Content-Encoding"
#define VAL2 "br"

#define KEY3 "Content-Language"
#define VAL3 "de-DE"

#define KEY4 "Content-Location"
#define VAL4 "/index.html"
#endif

//! Application buffer length
#define APP_BUFFER_LENGTH 2000

#define HTTP_SUCCESS_RESPONSE 1
#define HTTP_FAILURE_RESPONSE 2

//! End of data indications
// No data pending from host
#define HTTP_END_OF_DATA 1

#define HTTP_SYNC_RESPONSE  0
#define HTTP_ASYNC_RESPONSE 1

/******************************************************
 *               Variable Definitions
 ******************************************************/
const osThreadAttr_t http_client_thread_attributes = {
  .name       = "http_client_thread",
  .attr_bits  = 0,
  .cb_mem     = 0,
  .cb_size    = 0,
  .stack_mem  = 0,
  .stack_size = 3072,
  .priority   = osPriorityLow,
  .tz_module  = 0,
  .reserved   = 0,
};

const osThreadAttr_t gspi_thread_attributes = {
  .name       = "gspi_thread",
  .attr_bits  = 0,
  .cb_mem     = 0,
  .cb_size    = 0,
  .stack_mem  = 0,
  .stack_size = 2048,
  .priority   = osPriorityLow,
  .tz_module  = 0,
  .reserved   = 0,
};

osSemaphoreId_t http_client_thread_sem;
osSemaphoreId_t gspi_thread_sem;
osThreadId_t http_client_tid;
osThreadId_t gspi_tid;

static const sl_wifi_device_configuration_t http_client_configuration = {
  .boot_option = LOAD_NWP_FW,
  .mac_address = NULL,
  .band        = SL_SI91X_WIFI_BAND_2_4GHZ,
  .boot_config = { .oper_mode                  = SL_SI91X_CLIENT_MODE,
                   .coex_mode                  = SL_SI91X_WLAN_ONLY_MODE,
                   .feature_bit_map            = (SL_SI91X_FEAT_SECURITY_OPEN),
                   .tcp_ip_feature_bit_map     = (SL_SI91X_TCP_IP_FEAT_DHCPV4_CLIENT | SL_SI91X_TCP_IP_FEAT_HTTP_CLIENT
                                              | SL_SI91X_TCP_IP_FEAT_DNS_CLIENT | SL_SI91X_TCP_IP_FEAT_SSL),
                   .custom_feature_bit_map     = SL_SI91X_CUSTOM_FEAT_EXTENTION_VALID,
                   .ext_custom_feature_bit_map = (SL_SI91X_EXT_FEAT_XTAL_CLK | MEMORY_CONFIG
#ifdef SLI_SI917
                                                  | SL_SI91X_EXT_FEAT_FRONT_END_SWITCH_PINS_ULP_GPIO_4_5_0
#endif
                                                  ),
                   .bt_feature_bit_map      = 0,
                   .ble_feature_bit_map     = 0,
                   .ble_ext_feature_bit_map = 0,
                   .config_feature_bit_map  = 0 }
};

//! Application buffer
uint8_t app_buffer[APP_BUFFER_LENGTH] = { 0 };

//! Application buffer index
uint32_t app_buff_index = 0;

volatile uint8_t http_rsp_received = 0;
volatile uint8_t end_of_file       = 0;
sl_status_t callback_status        = SL_STATUS_OK;
/******************************************************
 *               Function Declarations
 ******************************************************/
static void application_start(void *argument);
sl_status_t http_client_application(void);
sl_status_t http_response_status(volatile uint8_t *response);
sl_status_t http_get_response_callback_handler(const sl_http_client_t *client,
                                               sl_http_client_event_t event,
                                               void *data,
                                               void *request_context);
#if !AMPAK_HTTP_GET_ONLY
sl_status_t http_put_response_callback_handler(const sl_http_client_t *client,
                                               sl_http_client_event_t event,
                                               void *data,
                                               void *request_context);
sl_status_t http_post_response_callback_handler(const sl_http_client_t *client,
                                                sl_http_client_event_t event,
                                                void *data,
                                                void *request_context);
#endif
static void reset_http_handles(void);

/******************************************************
 *               Function Definitions
 ******************************************************/
void app_init(const void *unused)
{
  UNUSED_PARAMETER(unused);

  printf("\r\nTick Freq: (%lu hz)\r\n",osKernelGetTickFreq());
  printf("SysTimer Freq: (%lu hz)\r\n",osKernelGetSysTimerFreq());
  printf("pdMS_TO_TICKS(1000): (%lu ticks)\r\n", pdMS_TO_TICKS(1000));

  if(mux_debug_init() == NULL)
  {
      printf("Failed to init mux log\r\n");
  }

  http_client_thread_sem = osSemaphoreNew(1, 0, NULL);
  if (http_client_thread_sem == NULL) {
      printf("Failed to create http_client_thread_sem\r\n");
      return;
  }
  gspi_thread_sem = osSemaphoreNew(1, 0, NULL);
  if (gspi_thread_sem == NULL) {
      printf("Failed to create gspi_thread_sem\r\n");
      return;
  }

  ringBuffer_Init(pRingBuff);

  if(osThreadNew((osThreadFunc_t)application_start, NULL, &http_client_thread_attributes) == NULL)
  {
      printf("Failed to new thread http client\r\n");
  }
  if(osThreadNew((osThreadFunc_t)gspi_task, NULL, &gspi_thread_attributes) == NULL)
  {
      printf("Failed to new thread gspi\r\n");
  }

  gspi_init();
}

static void application_start(void *argument)
{
  UNUSED_PARAMETER(argument);
  sl_status_t status;

//#if !AMPAK_HTTP_GET_ONLY
  status = sl_net_init(SL_NET_WIFI_CLIENT_INTERFACE, &http_client_configuration, NULL, NULL);
  if (status != SL_STATUS_OK) {
    MUX_LOG("Failed to start Wi-Fi client interface: 0x%lx\r\n", status);
    return;
  }
  MUX_LOG("Wi-Fi Init Success\r\n");

  status = sl_net_up(SL_NET_WIFI_CLIENT_INTERFACE, 0);
  if (status != SL_STATUS_OK) {
    MUX_LOG("Failed to bring Wi-Fi client interface up: 0x%lx\r\n", status);
    return;
  }
  MUX_LOG("Wi-Fi Client Connected\r\n");

  osSemaphoreRelease(http_client_thread_sem); //do once
//#endif //!AMPAK_HTTP_GET_ONLY

#if HTTPS_ENABLE && LOAD_CERTIFICATE
  // Load SSL CA certificate
  status = sl_net_set_credential(SL_NET_TLS_SERVER_CREDENTIAL_ID(CERTIFICATE_INDEX),
                                 SL_NET_SIGNING_CERTIFICATE,
                                 cacert,
                                 sizeof(cacert) - 1);
  if (status != SL_STATUS_OK) {
    MUX_LOG("\r\nLoading TLS CA certificate in to FLASH Failed, Error Code : 0x%lX\r\n", status);
    return;
  }
  MUX_LOG("\r\nLoad TLS CA certificate at index %d Success\r\n", CERTIFICATE_INDEX);
#endif

  do {
    osSemaphoreAcquire(http_client_thread_sem, osWaitForever);
    status = http_client_application();
    if (status != SL_STATUS_OK) {
      MUX_LOG("\r\nUnexpected error while HTTP client operation: 0x%lX\r\n", status);
      //return;
    }
  } while (1);

}

sl_status_t http_client_application(void)
{
  sl_status_t status                                  = SL_STATUS_OK;
  sl_http_client_t client_handle                      = 0;
  sl_http_client_configuration_t client_configuration = { 0 };
  sl_http_client_request_t client_request             = { 0 };
  int32_t total_put_data_len                          = sizeof(sl_index) - 1;
  int32_t offset                                      = 0;
  int32_t chunk_length                                = 0;

  //! Set HTTP Client credentials
  uint16_t username_length = strlen(HTTP_CLIENT_USERNAME);
  uint16_t password_length = strlen(HTTP_CLIENT_PASSWORD);
  uint32_t credential_size = sizeof(sl_http_client_credentials_t) + username_length + password_length;

  sl_http_client_credentials_t *client_credentials = (sl_http_client_credentials_t *)malloc(credential_size);
  SL_VERIFY_POINTER_OR_RETURN(client_credentials, SL_STATUS_ALLOCATION_FAILED);
  memset(client_credentials, 0, credential_size);
  client_credentials->username_length = username_length;
  client_credentials->password_length = password_length;

  memcpy(&client_credentials->data[0], HTTP_CLIENT_USERNAME, username_length);
  memcpy(&client_credentials->data[username_length], HTTP_CLIENT_PASSWORD, password_length);

  status = sl_net_set_credential(SL_NET_HTTP_CLIENT_CREDENTIAL_ID(0),
                                 SL_NET_HTTP_CLIENT_CREDENTIAL,
                                 client_credentials,
                                 credential_size);
  if (status != SL_STATUS_OK) {
    free(client_credentials);
    return status;
  }

  //! Fill HTTP client_configurations
  client_configuration.network_interface = SL_NET_WIFI_CLIENT_INTERFACE;
  client_configuration.ip_version        = IP_VERSION;
  client_configuration.http_version      = HTTP_VERSION;

#if HTTPS_ENABLE
  client_configuration.https_enable      = true;
  client_configuration.tls_version       = TLS_VERSION;
  client_configuration.certificate_index = CERTIFICATE_INDEX;
#endif

  //! Fill HTTP client_request configurations
  client_request.ip_address      = (uint8_t *)HTTP_SERVER_IP;
  client_request.port            = HTTP_PORT;
  client_request.resource        = (uint8_t *)HTTP_URL;
  client_request.extended_header = NULL;

  status = sl_http_client_init(&client_configuration, &client_handle);
  VERIFY_STATUS_AND_RETURN(status);
  MUX_LOG("HTTP Client init success\r\n");

#if !AMPAK_HTTP_GET_ONLY
  //! Configure HTTP PUT request
  client_request.http_method_type = SL_HTTP_PUT;
  client_request.body             = NULL;
  client_request.body_length      = total_put_data_len;

  //! Initialize callback method for HTTP PUT request
  status = sl_http_client_request_init(&client_request, http_put_response_callback_handler, "This is HTTP client");
  CLEAN_HTTP_CLIENT_IF_FAILED(status, &client_handle, HTTP_SYNC_RESPONSE);
  MUX_LOG("\r\nHTTP PUT request init success\r\n");
#endif

#if EXTENDED_HEADER_ENABLE
  //! Add extended headers
  status = sl_http_client_add_header(&client_request, KEY1, VAL1);
  CLEAN_HTTP_CLIENT_IF_FAILED(status, &client_handle, HTTP_SYNC_RESPONSE);

  status = sl_http_client_add_header(&client_request, KEY2, VAL2);
  CLEAN_HTTP_CLIENT_IF_FAILED(status, &client_handle, HTTP_SYNC_RESPONSE);

  status = sl_http_client_add_header(&client_request, KEY3, VAL3);
  CLEAN_HTTP_CLIENT_IF_FAILED(status, &client_handle, HTTP_SYNC_RESPONSE);

  status = sl_http_client_add_header(&client_request, KEY4, VAL4);
  CLEAN_HTTP_CLIENT_IF_FAILED(status, &client_handle, HTTP_SYNC_RESPONSE);
#endif

#if !AMPAK_HTTP_GET_ONLY
  //! Send HTTP PUT request
  status = sl_http_client_send_request(&client_handle, &client_request);
  if (status == SL_STATUS_IN_PROGRESS) {
    status = http_response_status(&http_rsp_received);
    CLEAN_HTTP_CLIENT_IF_FAILED(status, &client_handle, HTTP_ASYNC_RESPONSE);
  } else {
    CLEAN_HTTP_CLIENT_IF_FAILED(status, &client_handle, HTTP_SYNC_RESPONSE);
  }

  //! Write HTTP PUT data
  while (!end_of_file) {
    //! Get the current length that you want to send
    chunk_length = ((total_put_data_len - offset) > SL_HTTP_CLIENT_MAX_WRITE_BUFFER_LENGTH)
                     ? SL_HTTP_CLIENT_MAX_WRITE_BUFFER_LENGTH
                     : (total_put_data_len - offset);

    status = sl_http_client_write_chunked_data(&client_handle, (uint8_t *)(sl_index + offset), chunk_length, 0);

    if (status == SL_STATUS_IN_PROGRESS) {
      status = http_response_status(&http_rsp_received);
      CLEAN_HTTP_CLIENT_IF_FAILED(status, &client_handle, HTTP_ASYNC_RESPONSE);

      offset += chunk_length;
    } else {
      CLEAN_HTTP_CLIENT_IF_FAILED(status, &client_handle, HTTP_SYNC_RESPONSE);
    }
  }

  MUX_LOG("\r\nHTTP PUT request Success!\r\n");
#endif
  reset_http_handles(); // reset once

  //! Configure HTTP GET request
  client_request.http_method_type = SL_HTTP_GET;

  //! Initialize callback method for HTTP GET request
  status = sl_http_client_request_init(&client_request, http_get_response_callback_handler, "This is HTTP client");
  CLEAN_HTTP_CLIENT_IF_FAILED(status, &client_handle, HTTP_SYNC_RESPONSE);
  MUX_LOG("HTTP Get request init success\r\n");

  MUX_LOG("Tick Freq: (%lu hz)\r\n",osKernelGetTickFreq());
  MUX_LOG("SysTimer Freq: (%lu hz)\r\n",osKernelGetSysTimerFreq());
  MUX_LOG("pdMS_TO_TICKS(1000): (%lu ticks)\r\n", pdMS_TO_TICKS(1000));

  uint32_t starttime = osKernelGetTickCount();
  //! Send HTTP GET request
  status = sl_http_client_send_request(&client_handle, &client_request);
  if (status == SL_STATUS_IN_PROGRESS) {
    status = http_response_status(&http_rsp_received);
    CLEAN_HTTP_CLIENT_IF_FAILED(status, &client_handle, HTTP_ASYNC_RESPONSE);
  } else {
    CLEAN_HTTP_CLIENT_IF_FAILED(status, &client_handle, HTTP_SYNC_RESPONSE);
  }

  uint32_t duration = osKernelGetTickCount() - starttime;

  MUX_LOG("Length\t\t| Time\t\t| Datarate\r\n");
  MUX_LOG("%lu\t\t| %lu\t\t| %lu\r\n", app_buff_index, duration, app_buff_index / duration);

  MUX_LOG("HTTP GET request Success\r\n");
  reset_http_handles(); // reset twice

#if !AMPAK_HTTP_GET_ONLY
  //! Configure HTTP POST request
  client_request.http_method_type = SL_HTTP_POST;
  client_request.body             = (uint8_t *)HTTP_DATA;
  client_request.body_length      = strlen(HTTP_DATA);

  //! Initialize callback method for HTTP POST request
  status = sl_http_client_request_init(&client_request, http_post_response_callback_handler, "This is HTTP client");
  CLEAN_HTTP_CLIENT_IF_FAILED(status, &client_handle, HTTP_SYNC_RESPONSE);
  MUX_LOG("HTTP Post request init success\r\n");

  //! Send HTTP POST request
  status = sl_http_client_send_request(&client_handle, &client_request);
  if (status == SL_STATUS_IN_PROGRESS) {
    status = http_response_status(&http_rsp_received);
    CLEAN_HTTP_CLIENT_IF_FAILED(status, &client_handle, HTTP_ASYNC_RESPONSE);
  } else {
    CLEAN_HTTP_CLIENT_IF_FAILED(status, &client_handle, HTTP_SYNC_RESPONSE);
  }

  MUX_LOG("HTTP POST request Success\r\n");
  reset_http_handles();
#endif

#if EXTENDED_HEADER_ENABLE
  status = sl_http_client_delete_all_headers(&client_request);
  CLEAN_HTTP_CLIENT_IF_FAILED(status, &client_handle, HTTP_SYNC_RESPONSE);
#endif

  status = sl_http_client_deinit(&client_handle);
  VERIFY_STATUS_AND_RETURN(status);
  MUX_LOG("HTTP Client deinit success\r\n");
#if !AMPAK_HTTP_GET_ONLY
  free(client_credentials);
#endif

  return status; // no return maybe
}

sl_status_t http_get_response_callback_handler(const sl_http_client_t *client,
                                               sl_http_client_event_t event,
                                               void *data,
                                               void *request_context)
{
  UNUSED_PARAMETER(client);
  UNUSED_PARAMETER(event);

  sl_http_client_response_t *get_response = (sl_http_client_response_t *)data;
  callback_status                         = get_response->status;
#if AMPAK_HTTP_RESPONSE_CHECK
  MUX_LOG(
      "\r\n[HTTP GET RESPONSE] Status: 0x%X | GET response: %u | End of data: %lu | Data Length: %u | Request Context: %s\r\n",
      get_response->status,
      get_response->http_response_code,
      get_response->end_of_data,
      get_response->data_length,
      (char *)request_context);
#endif
  if (get_response->status != SL_STATUS_OK
      || (get_response->http_response_code >= 400 && get_response->http_response_code <= 599
          && get_response->http_response_code != 0))
  {
    http_rsp_received = HTTP_FAILURE_RESPONSE;
    MUX_LOG("get_response->http_response_code: %u\r\n", get_response->http_response_code);
    return get_response->status;
  }

  if (!get_response->end_of_data)
  {
    //memcpy(app_buffer + app_buff_index, get_response->data_buffer, get_response->data_length);
    // copy to ring buffer for sd card write

    if(ringBuffer_check_ready_to_write(pRingBuff) == RINGBUFF_OK)
    {
      if(ringBuffer_write(pRingBuff, get_response->data_buffer, get_response->data_length) != RINGBUFF_OK)
      {
        http_debug_log("X");
        return SL_STATUS_FAIL;
      }
    }
    else
    {
      http_debug_log("x");
      return SL_STATUS_FAIL;
    }

    app_buff_index += get_response->data_length;
    //http_debug_log(">");
  }
  else
  {
    if (get_response->data_length)
    {
      //memcpy(app_buffer + app_buff_index, get_response->data_buffer, get_response->data_length);
      // copy to ring buffer for sd card write
      if(ringBuffer_check_ready_to_write(pRingBuff) == RINGBUFF_OK)
      {
        if(ringBuffer_write(pRingBuff, get_response->data_buffer, get_response->data_length) != RINGBUFF_OK)
        {
          http_debug_log("Z");
          return SL_STATUS_FAIL;
        }
      }
      else
      {
        http_debug_log("z");
        return SL_STATUS_FAIL;
      }

      /* TODO clear buffer last data */

      app_buff_index += get_response->data_length;
      http_debug_log(".\r\n");
    }
    http_rsp_received = HTTP_SUCCESS_RESPONSE;
  }

  return SL_STATUS_OK;
}
#if !AMPAK_HTTP_GET_ONLY
sl_status_t http_put_response_callback_handler(const sl_http_client_t *client,
                                               sl_http_client_event_t event,
                                               void *data,
                                               void *request_context)
{
  UNUSED_PARAMETER(client);
  UNUSED_PARAMETER(event);

  sl_http_client_response_t *put_response = (sl_http_client_response_t *)data;
  callback_status                         = put_response->status;

  MUX_LOG("\r\n===========HTTP PUT RESPONSE START===========\r\n");
  MUX_LOG(
    "\r\n> Status: 0x%X\n> PUT response: %u\n> End of data: %lu\n> Data Length: %u\n> Request Context: %s\r\n",
    put_response->status,
    put_response->http_response_code,
    put_response->end_of_data,
    put_response->data_length,
    (char *)request_context);

  if (put_response->status != SL_STATUS_OK) {
    http_rsp_received = 2;
    return put_response->status;
  }

  if (put_response->end_of_data & HTTP_END_OF_DATA) {
    if (put_response->data_length) {
      memcpy(app_buffer + app_buff_index, put_response->data_buffer, put_response->data_length);
      app_buff_index += put_response->data_length;
    }
    http_rsp_received = HTTP_SUCCESS_RESPONSE;
    end_of_file       = HTTP_SUCCESS_RESPONSE;
  } else {
    memcpy(app_buffer + app_buff_index, put_response->data_buffer, put_response->data_length);
    app_buff_index += put_response->data_length;
    http_rsp_received = HTTP_SUCCESS_RESPONSE;
  }

  return SL_STATUS_OK;
}

sl_status_t http_post_response_callback_handler(const sl_http_client_t *client,
                                                sl_http_client_event_t event,
                                                void *data,
                                                void *request_context)
{
  UNUSED_PARAMETER(client);
  UNUSED_PARAMETER(event);

  sl_http_client_response_t *post_response = (sl_http_client_response_t *)data;
  callback_status                          = post_response->status;

  MUX_LOG("\r\n===========HTTP POST RESPONSE START===========\r\n");
  MUX_LOG(
    "\r\n> Status: 0x%X\n> POST response: %u\n> End of data: %lu\n> Data Length: %u\n> Request Context: %s\r\n",
    post_response->status,
    post_response->http_response_code,
    post_response->end_of_data,
    post_response->data_length,
    (char *)request_context);

  if (post_response->status != SL_STATUS_OK
      || (post_response->http_response_code >= 400 && post_response->http_response_code <= 599
          && post_response->http_response_code != 0)) {
    http_rsp_received = HTTP_FAILURE_RESPONSE;
    return post_response->status;
  }

  if (post_response->end_of_data) {
    http_rsp_received = 1;
  }

  return SL_STATUS_OK;
}
#endif
sl_status_t http_response_status(volatile uint8_t *response)
{
  while (!(*response)) {
    /* Wait till response arrives */
  }

  if (*response != HTTP_SUCCESS_RESPONSE) {
    return SL_STATUS_FAIL;
  }

  // Reset response
  *response = 0;

  return SL_STATUS_OK;
}

static void reset_http_handles(void)
{
  app_buff_index = 0;
  end_of_file    = 0;
}

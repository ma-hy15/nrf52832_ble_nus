/**
 * Copyright (c) 2014 - 2019, Nordic Semiconductor ASA
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form, except as embedded into a Nordic
 *    Semiconductor ASA integrated circuit in a product or a software update for
 *    such product, must reproduce the above copyright notice, this list of
 *    conditions and the following disclaimer in the documentation and/or other
 *    materials provided with the distribution.
 *
 * 3. Neither the name of Nordic Semiconductor ASA nor the names of its
 *    contributors may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * 4. This software, with or without modification, must only be used with a
 *    Nordic Semiconductor ASA integrated circuit.
 *
 * 5. Any software provided in binary form under this license must not be reverse
 *    engineered, decompiled, modified and/or disassembled.
 *
 * THIS SOFTWARE IS PROVIDED BY NORDIC SEMICONDUCTOR ASA "AS IS" AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL NORDIC SEMICONDUCTOR ASA OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
 * GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/** @file
 *
 * @defgroup ble_sdk_uart_over_ble_main main.c
 * @{
 * @ingroup  ble_sdk_app_nus_eval
 * @brief    UART over BLE application main file.
 *
 * This file contains the source code for a sample application that uses the Nordic UART service.
 * This application uses the @ref srvlib_conn_params module.
 */


#include <stdint.h>
#include <string.h>
#include "nordic_common.h"
#include "nrf.h"
#include "ble_hci.h"
#include "ble_advdata.h"
#include "ble_advertising.h"
#include "ble_conn_params.h"
#include "nrf_sdh.h"
#include "nrf_sdh_soc.h"
#include "nrf_sdh_ble.h"
#include "nrf_ble_gatt.h"
#include "nrf_ble_qwr.h"
#include "app_timer.h"
#include "ble_nus.h"
#include "app_uart.h"
#include "app_util_platform.h"
#include "bsp_btn_ble.h"
#include "nrf_pwr_mgmt.h"
#include "nrf_delay.h"

//ADC需要引用的头文件
#include "nrf_drv_saadc.h"
//ppi需要引用的头文件
#include "nrf_drv_ppi.h"
//gpiote需要引用的头文件
#include "nrf_drv_gpiote.h"
//定时器需要引用的头文件
#include "nrf_drv_timer.h"
//gpiote需要引用的头文件
#include "nrf_drv_gpiote.h"
//看门狗需要引用的头文件
#include "nrfx_wdt.h"
#include "nrf_drv_clock.h"
//平方根
#include "math.h"


#if defined (UART_PRESENT)
#include "nrf_uart.h"
#endif
#if defined (UARTE_PRESENT)
#include "nrf_uarte.h"
#endif

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"
#include "nrf_queue.h"

#define APP_BLE_CONN_CFG_TAG            1                                           /**< A tag identifying the SoftDevice BLE configuration. */

#define DEVICE_NAME                     "Nordic_UART"                               /**< Name of device. Will be included in the advertising data. */
#define NUS_SERVICE_UUID_TYPE           BLE_UUID_TYPE_VENDOR_BEGIN                  /**< UUID type for the Nordic UART Service (vendor specific). */

#define APP_BLE_OBSERVER_PRIO           3                                           /**< Application's BLE observer priority. You shouldn't need to modify this value. */

#define APP_ADV_FAST_INTERVAL           64                                          /**< The advertising interval (in units of 0.625 ms. This value corresponds to 40 ms). */
#define APP_ADV_SLOW_INTERVAL           1600                                        /**< The advertising interval (in units of 0.625 ms. This value corresponds to 4s). */

#define APP_ADV_FAST_DURATION           2000                                        /**< The advertising duration (20 seconds) in units of 10 milliseconds. */
#define APP_ADV_SLOW_DURATION           0                                           /**< The advertising duration (WQ seconds) in units of 10 milliseconds. */

#define MIN_CONN_INTERVAL               MSEC_TO_UNITS(20, UNIT_1_25_MS)             /**< Minimum acceptable connection interval (20 ms), Connection interval uses 1.25 ms units. */
#define MAX_CONN_INTERVAL               MSEC_TO_UNITS(40, UNIT_1_25_MS)             /**< Maximum acceptable connection interval (75 ms), Connection interval uses 1.25 ms units. */
#define SLAVE_LATENCY                   0                                           /**< Slave latency. */
#define CONN_SUP_TIMEOUT                MSEC_TO_UNITS(4000, UNIT_10_MS)             /**< Connection supervisory timeout (4 seconds), Supervision Timeout uses 10 ms units. */
#define FIRST_CONN_PARAMS_UPDATE_DELAY  APP_TIMER_TICKS(5000)                       /**< Time from initiating event (connect or start of notification) to first time sd_ble_gap_conn_param_update is called (5 seconds). */
#define NEXT_CONN_PARAMS_UPDATE_DELAY   APP_TIMER_TICKS(30000)                      /**< Time between each call to sd_ble_gap_conn_param_update after the first call (30 seconds). */
#define MAX_CONN_PARAMS_UPDATE_COUNT    3                                           /**< Number of attempts before giving up the connection parameter negotiation. */

#define DEAD_BEEF                       0xDEADBEEF                                  /**< Value used as error code on stack dump, can be used to identify stack location on stack unwind. */

#define UART_TX_BUF_SIZE                256                                         /**< UART TX buffer size. */
#define UART_RX_BUF_SIZE                256                                         /**< UART RX buffer size. */

#define SAMPLE_BUFFER_DOTS 100
#define CHANNEL 4
#define SAMPLE_BUFFER_LEN SAMPLE_BUFFER_DOTS*CHANNEL
#define HC 5 																																			/**< 稳态时的缓冲数据长度. */

#define TPL5010_WAKE 24
#define TPL5010_DONE 14
#define CONTROL_VCC 6

BLE_NUS_DEF(m_nus, NRF_SDH_BLE_TOTAL_LINK_COUNT);                                   /**< BLE NUS service instance. */
NRF_BLE_GATT_DEF(m_gatt);                                                           /**< GATT module instance. */
NRF_BLE_QWR_DEF(m_qwr);                                                             /**< Context for the Queued Write module.*/
BLE_ADVERTISING_DEF(m_advertising);                                                 /**< Advertising module instance. */

static uint16_t   m_conn_handle          = BLE_CONN_HANDLE_INVALID;                 /**< Handle of the current connection. */
static uint16_t   m_ble_nus_max_data_len = BLE_GATT_ATT_MTU_DEFAULT - 3;            /**< Maximum length of data (in bytes) that can be transmitted to the peer by the Nordic UART service module. */
static ble_uuid_t m_adv_uuids[]          =                                          /**< Universally unique service identifier. */
{
    {BLE_UUID_NUS_SERVICE, NUS_SERVICE_UUID_TYPE}
};

/**
** custom code
**/

//#define APP_QUEUE
//void ble_data_send_with_queue(void);
//typedef struct {
//    uint8_t * p_data;
//    uint16_t length;
//} buffer_t;

//NRF_QUEUE_DEF(buffer_t, m_buf_queue, 30, NRF_QUEUE_MODE_NO_OVERFLOW);

//APP_TIMER_DEF(m_timer_speed);
//uint8_t m_data_array[6300];
//uint32_t m_len_sent;
//uint32_t m_cnt_7ms;

// 保存应用程序向驱动程序申请的PPI通道编号
nrf_ppi_channel_t my_ppi_channel;
// 采样缓存数组
static nrf_saadc_value_t m_buffer_pool[2][SAMPLE_BUFFER_LEN];
// 保存进入ADC事件回调的次数
static uint32_t m_adc_evt_counter=0;
// 保存成功发送数据的次数
static uint32_t m_adv_success_send=0;
// 驱动实例定义，使用TIMER2
const nrfx_timer_t MY_TIMER = NRFX_TIMER_INSTANCE(2);
// 保存申请的喂狗通道
nrfx_wdt_channel_id my_channel_id;
APP_TIMER_DEF(wdt_timer_speed);
//APP_TIMER_DEF(advertising_timer);
// 保存数据指针用于TX事件
uint8_t * adc_tx_data;
uint16_t ave_data[HC][CHANNEL]; //稳态数据保存
uint16_t hc_num=0;
uint16_t length;
static uint8_t sensor_state = 0;

void sensor_init();
void sensor_uninit();


/**@brief Function for assert macro callback.
 *
 * @details This function will be called in case of an assert in the SoftDevice.
 *
 * @warning This handler is an example only and does not fit a final product. You need to analyse
 *          how your product is supposed to react in case of Assert.
 * @warning On assert from the SoftDevice, the system can only recover on reset.
 *
 * @param[in] line_num    Line number of the failing ASSERT call.
 * @param[in] p_file_name File name of the failing ASSERT call.
 */
void assert_nrf_callback(uint16_t line_num, const uint8_t * p_file_name)
{
    app_error_handler(DEAD_BEEF, line_num, p_file_name);
}

/**@brief Function for initializing the timer module.
 */
static void timers_init(void)
{
    ret_code_t err_code = app_timer_init();
    APP_ERROR_CHECK(err_code);
}

/**@brief Function for the GAP initialization.
 *
 * @details This function will set up all the necessary GAP (Generic Access Profile) parameters of
 *          the device. It also sets the permissions and appearance.
 */
static void gap_params_init(void)
{
    uint32_t                err_code;
    ble_gap_conn_params_t   gap_conn_params;
    ble_gap_conn_sec_mode_t sec_mode;

    BLE_GAP_CONN_SEC_MODE_SET_OPEN(&sec_mode);

    err_code = sd_ble_gap_device_name_set(&sec_mode,
                                          (const uint8_t *) DEVICE_NAME,
                                          strlen(DEVICE_NAME));
    APP_ERROR_CHECK(err_code);

    memset(&gap_conn_params, 0, sizeof(gap_conn_params));

    gap_conn_params.min_conn_interval = MIN_CONN_INTERVAL;
    gap_conn_params.max_conn_interval = MAX_CONN_INTERVAL;
    gap_conn_params.slave_latency     = SLAVE_LATENCY;
    gap_conn_params.conn_sup_timeout  = CONN_SUP_TIMEOUT;

    err_code = sd_ble_gap_ppcp_set(&gap_conn_params);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling Queued Write Module errors.
 *
 * @details A pointer to this function will be passed to each service which may need to inform the
 *          application about an error.
 *
 * @param[in]   nrf_error   Error code containing information about what went wrong.
 */
static void nrf_qwr_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for handling the data from the Nordic UART Service.
 *
 * @details This function will process the data received from the Nordic UART BLE Service and send
 *          it to the UART module.
 *
 * @param[in] p_evt       Nordic UART Service event.
 */
/**@snippet [Handling the data received over BLE] */
static void nus_data_handler(ble_nus_evt_t * p_evt)
{

    if (p_evt->type == BLE_NUS_EVT_RX_DATA)
    {
//        uint32_t err_code;

//        NRF_LOG_DEBUG("Received data from BLE NUS. Writing data on UART.");
//        NRF_LOG_HEXDUMP_DEBUG(p_evt->params.rx_data.p_data, p_evt->params.rx_data.length);

//        for (uint32_t i = 0; i < p_evt->params.rx_data.length; i++)
//        {
//            do
//            {
//                err_code = app_uart_put(p_evt->params.rx_data.p_data[i]);
//                if ((err_code != NRF_SUCCESS) && (err_code != NRF_ERROR_BUSY))
//                {
//                    NRF_LOG_ERROR("Failed receiving NUS message. Error 0x%x. ", err_code);
//                    APP_ERROR_CHECK(err_code);
//                }
//            } while (err_code == NRF_ERROR_BUSY);
//        }
//        if (p_evt->params.rx_data.p_data[p_evt->params.rx_data.length - 1] == '\r')
//        {
//            while (app_uart_put('\n') == NRF_ERROR_BUSY);
//        }
    }
		else if (p_evt->type == BLE_NUS_EVT_COMM_STARTED)
		{
			//使能PPI通道
			nrfx_timer_enable(&MY_TIMER);
			APP_ERROR_CHECK(nrfx_ppi_channel_enable(my_ppi_channel));		
			hc_num=0;
		}
		else if (p_evt->type == BLE_NUS_EVT_COMM_STOPPED)
		{
			//关闭PPI通道
			APP_ERROR_CHECK(nrfx_ppi_channel_disable(my_ppi_channel));
			nrfx_timer_disable(&MY_TIMER);
			hc_num=0;
		}
		else if (p_evt->type == BLE_NUS_EVT_TX_RDY)
		{
#ifndef APP_QUEUE			
//			ret_code_t err_code;
//			uint16_t length;	
//			
//			//sending code lines
//			length = m_ble_nus_max_data_len;	
//			do
//			{					
//				err_code = ble_nus_data_send(&m_nus, m_data_array, &length, m_conn_handle);
//				if ( (err_code != NRF_ERROR_INVALID_STATE) && (err_code != NRF_ERROR_RESOURCES) &&
//						 (err_code != NRF_ERROR_NOT_FOUND) )
//				{
//						APP_ERROR_CHECK(err_code);
//				}
//				if (err_code == NRF_SUCCESS)
//				{
//					m_len_sent += length; 	
//					m_data_array[0]++;
//					m_data_array[length-1]++;	
//				}
//			} while (err_code == NRF_SUCCESS);
#else
      ble_data_send_with_queue();
#endif
		}

}
/**@snippet [Handling the data received over BLE] */


/**@brief Function for initializing services that will be used by the application.
 */
static void services_init(void)
{
    uint32_t           err_code;
    ble_nus_init_t     nus_init;
    nrf_ble_qwr_init_t qwr_init = {0};

    // Initialize Queued Write Module.
    qwr_init.error_handler = nrf_qwr_error_handler;

    err_code = nrf_ble_qwr_init(&m_qwr, &qwr_init);
    APP_ERROR_CHECK(err_code);

    // Initialize NUS.
    memset(&nus_init, 0, sizeof(nus_init));

    nus_init.data_handler = nus_data_handler;

    err_code = ble_nus_init(&m_nus, &nus_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling an event from the Connection Parameters Module.
 *
 * @details This function will be called for all events in the Connection Parameters Module
 *          which are passed to the application.
 *
 * @note All this function does is to disconnect. This could have been done by simply setting
 *       the disconnect_on_fail config parameter, but instead we use the event handler
 *       mechanism to demonstrate its use.
 *
 * @param[in] p_evt  Event received from the Connection Parameters Module.
 */
static void on_conn_params_evt(ble_conn_params_evt_t * p_evt)
{
    uint32_t err_code;

    if (p_evt->evt_type == BLE_CONN_PARAMS_EVT_FAILED)
    {
        err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_CONN_INTERVAL_UNACCEPTABLE);
        APP_ERROR_CHECK(err_code);
    }
}


/**@brief Function for handling errors from the Connection Parameters module.
 *
 * @param[in] nrf_error  Error code containing information about what went wrong.
 */
static void conn_params_error_handler(uint32_t nrf_error)
{
    APP_ERROR_HANDLER(nrf_error);
}


/**@brief Function for initializing the Connection Parameters module.
 */
static void conn_params_init(void)
{
    uint32_t               err_code;
    ble_conn_params_init_t cp_init;

    memset(&cp_init, 0, sizeof(cp_init));

    cp_init.p_conn_params                  = NULL;
    cp_init.first_conn_params_update_delay = FIRST_CONN_PARAMS_UPDATE_DELAY;
    cp_init.next_conn_params_update_delay  = NEXT_CONN_PARAMS_UPDATE_DELAY;
    cp_init.max_conn_params_update_count   = MAX_CONN_PARAMS_UPDATE_COUNT;
    cp_init.start_on_notify_cccd_handle    = BLE_GATT_HANDLE_INVALID;
    cp_init.disconnect_on_fail             = false;
    cp_init.evt_handler                    = on_conn_params_evt;
    cp_init.error_handler                  = conn_params_error_handler;

    err_code = ble_conn_params_init(&cp_init);
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for putting the chip into sleep mode.
 *
 * @note This function will not return.
 */
static void sleep_mode_enter(void)
{
    uint32_t err_code = bsp_indication_set(BSP_INDICATE_IDLE);
    APP_ERROR_CHECK(err_code);

    // Prepare wakeup buttons.
    err_code = bsp_btn_ble_sleep_mode_prepare();
    APP_ERROR_CHECK(err_code);

    // Go to system-off mode (this function will not return; wakeup will cause a reset).
    err_code = sd_power_system_off();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling advertising events.
 *
 * @details This function will be called for advertising events which are passed to the application.
 *
 * @param[in] ble_adv_evt  Advertising event.
 */
static void on_adv_evt(ble_adv_evt_t ble_adv_evt)
{
    uint32_t err_code;

    switch (ble_adv_evt)
    {
        case BLE_ADV_EVT_FAST:
						NRF_LOG_INFO("FAST MODE");
            err_code = bsp_indication_set(BSP_INDICATE_ADVERTISING);
            APP_ERROR_CHECK(err_code);
            break;
				case BLE_ADV_EVT_SLOW:
						NRF_LOG_INFO("SLOW MODE");
						break;
        case BLE_ADV_EVT_IDLE:
            sleep_mode_enter();
            break;
        default:
            break;
    }
}


/**@brief Function for handling BLE events.
 *
 * @param[in]   p_ble_evt   Bluetooth stack event.
 * @param[in]   p_context   Unused.
 */
static void ble_evt_handler(ble_evt_t const * p_ble_evt, void * p_context)
{
    uint32_t err_code;

    switch (p_ble_evt->header.evt_id)
    {
        case BLE_GAP_EVT_CONNECTED:
            NRF_LOG_INFO("Connected");
            err_code = bsp_indication_set(BSP_INDICATE_CONNECTED);
            APP_ERROR_CHECK(err_code);
            m_conn_handle = p_ble_evt->evt.gap_evt.conn_handle;
            err_code = nrf_ble_qwr_conn_handle_assign(&m_qwr, m_conn_handle);
            APP_ERROR_CHECK(err_code);
						sensor_init();
            break;

        case BLE_GAP_EVT_DISCONNECTED:
            NRF_LOG_INFO("Disconnected");
            // LED indication will be changed when advertising starts.
            m_conn_handle = BLE_CONN_HANDLE_INVALID;
						//关闭PPI通道
						APP_ERROR_CHECK(nrfx_ppi_channel_disable(my_ppi_channel));
						nrfx_timer_disable(&MY_TIMER);
						m_adc_evt_counter=0;
						m_adv_success_send=0;
						hc_num=0;
						sensor_uninit();
            break;

        case BLE_GAP_EVT_PHY_UPDATE_REQUEST:
        {
            NRF_LOG_DEBUG("PHY update request.");
            ble_gap_phys_t const phys =
            {
                .rx_phys = BLE_GAP_PHY_AUTO,
                .tx_phys = BLE_GAP_PHY_AUTO,
            };
            err_code = sd_ble_gap_phy_update(p_ble_evt->evt.gap_evt.conn_handle, &phys);
            APP_ERROR_CHECK(err_code);
        } break;

        case BLE_GAP_EVT_SEC_PARAMS_REQUEST:
            // Pairing not supported
            err_code = sd_ble_gap_sec_params_reply(m_conn_handle, BLE_GAP_SEC_STATUS_PAIRING_NOT_SUPP, NULL, NULL);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_SYS_ATTR_MISSING:
            // No system attributes have been stored.
            err_code = sd_ble_gatts_sys_attr_set(m_conn_handle, NULL, 0, 0);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTC_EVT_TIMEOUT:
            // Disconnect on GATT Client timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gattc_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        case BLE_GATTS_EVT_TIMEOUT:
            // Disconnect on GATT Server timeout event.
            err_code = sd_ble_gap_disconnect(p_ble_evt->evt.gatts_evt.conn_handle,
                                             BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            APP_ERROR_CHECK(err_code);
            break;

        default:
            // No implementation needed.
            break;
    }
}


/**@brief Function for the SoftDevice initialization.
 *
 * @details This function initializes the SoftDevice and the BLE event interrupt.
 */
static void ble_stack_init(void)
{
    ret_code_t err_code;

    err_code = nrf_sdh_enable_request();
    APP_ERROR_CHECK(err_code);

    // Configure the BLE stack using the default settings.
    // Fetch the start address of the application RAM.
    uint32_t ram_start = 0;
    err_code = nrf_sdh_ble_default_cfg_set(APP_BLE_CONN_CFG_TAG, &ram_start);
    APP_ERROR_CHECK(err_code);

    // Enable BLE stack.
    err_code = nrf_sdh_ble_enable(&ram_start);
    APP_ERROR_CHECK(err_code);

    // Register a handler for BLE events.
    NRF_SDH_BLE_OBSERVER(m_ble_observer, APP_BLE_OBSERVER_PRIO, ble_evt_handler, NULL);
}


/**@brief Function for handling events from the GATT library. */
void gatt_evt_handler(nrf_ble_gatt_t * p_gatt, nrf_ble_gatt_evt_t const * p_evt)
{
    if ((m_conn_handle == p_evt->conn_handle) && (p_evt->evt_id == NRF_BLE_GATT_EVT_ATT_MTU_UPDATED))
    {
        m_ble_nus_max_data_len = p_evt->params.att_mtu_effective - OPCODE_LENGTH - HANDLE_LENGTH;
        NRF_LOG_INFO("Data len is set to 0x%X(%d)", m_ble_nus_max_data_len, m_ble_nus_max_data_len);
    }
    NRF_LOG_DEBUG("ATT MTU exchange completed. central 0x%x peripheral 0x%x",
                  p_gatt->att_mtu_desired_central,
                  p_gatt->att_mtu_desired_periph);
}


/**@brief Function for initializing the GATT library. */
void gatt_init(void)
{
    ret_code_t err_code;

    err_code = nrf_ble_gatt_init(&m_gatt, gatt_evt_handler);
    APP_ERROR_CHECK(err_code);

    //err_code = nrf_ble_gatt_att_mtu_periph_set(&m_gatt, NRF_SDH_BLE_GATT_MAX_MTU_SIZE);
    //APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling events from the BSP module.
 *
 * @param[in]   event   Event generated by button press.
 */
void bsp_event_handler(bsp_event_t event)
{
    uint32_t err_code;
    switch (event)
    {
        case BSP_EVENT_SLEEP:
            sleep_mode_enter();
            break;

        case BSP_EVENT_DISCONNECT:
            err_code = sd_ble_gap_disconnect(m_conn_handle, BLE_HCI_REMOTE_USER_TERMINATED_CONNECTION);
            if (err_code != NRF_ERROR_INVALID_STATE)
            {
                APP_ERROR_CHECK(err_code);
            }
            break;

        case BSP_EVENT_WHITELIST_OFF:
            if (m_conn_handle == BLE_CONN_HANDLE_INVALID)
            {
                err_code = ble_advertising_restart_without_whitelist(&m_advertising);
                if (err_code != NRF_ERROR_INVALID_STATE)
                {
                    APP_ERROR_CHECK(err_code);
                }
            }
            break;

        default:
            break;
    }
}


/**@brief   Function for handling app_uart events.
 *
 * @details This function will receive a single character from the app_uart module and append it to
 *          a string. The string will be be sent over BLE when the last character received was a
 *          'new line' '\n' (hex 0x0A) or if the string has reached the maximum data length.
 */
/**@snippet [Handling the data received over UART] */
void uart_event_handle(app_uart_evt_t * p_event)
{
    static uint8_t data_array[BLE_NUS_MAX_DATA_LEN];
    static uint8_t index = 0;
    uint32_t       err_code;

    switch (p_event->evt_type)
    {
        case APP_UART_DATA_READY:
            UNUSED_VARIABLE(app_uart_get(&data_array[index]));
            index++;

            if ((data_array[index - 1] == '\n') ||
                (data_array[index - 1] == '\r') ||
                (index >= m_ble_nus_max_data_len))
            {
                if (index > 1)
                {
                    NRF_LOG_DEBUG("Ready to send data over BLE NUS");
                    NRF_LOG_HEXDUMP_DEBUG(data_array, index);

                    do
                    {
                        uint16_t length = (uint16_t)index;
                        err_code = ble_nus_data_send(&m_nus, data_array, &length, m_conn_handle);
                        if ((err_code != NRF_ERROR_INVALID_STATE) &&
                            (err_code != NRF_ERROR_RESOURCES) &&
                            (err_code != NRF_ERROR_NOT_FOUND))
                        {
                            APP_ERROR_CHECK(err_code);
                        }
                    } while (err_code == NRF_ERROR_RESOURCES);
                }

                index = 0;
            }
            break;

        case APP_UART_COMMUNICATION_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_communication);
            break;

        case APP_UART_FIFO_ERROR:
            APP_ERROR_HANDLER(p_event->data.error_code);
            break;

        default:
            break;
    }
}
/**@snippet [Handling the data received over UART] */


/**@brief  Function for initializing the UART module.
 */
/**@snippet [UART Initialization] */
static void uart_init(void)
{
    uint32_t                     err_code;
    app_uart_comm_params_t const comm_params =
    {
        .rx_pin_no    = RX_PIN_NUMBER,
        .tx_pin_no    = TX_PIN_NUMBER,
        .rts_pin_no   = RTS_PIN_NUMBER,
        .cts_pin_no   = CTS_PIN_NUMBER,
        .flow_control = APP_UART_FLOW_CONTROL_DISABLED,
        .use_parity   = false,
#if defined (UART_PRESENT)
        .baud_rate    = NRF_UART_BAUDRATE_115200
#else
        .baud_rate    = NRF_UARTE_BAUDRATE_115200
#endif
    };

    APP_UART_FIFO_INIT(&comm_params,
                       UART_RX_BUF_SIZE,
                       UART_TX_BUF_SIZE,
                       uart_event_handle,
                       APP_IRQ_PRIORITY_LOWEST,
                       err_code);
    APP_ERROR_CHECK(err_code);
}
/**@snippet [UART Initialization] */


/**@brief Function for initializing the Advertising functionality.
 */
static void advertising_init(void)
{
    uint32_t               err_code;
    ble_advertising_init_t init;

    memset(&init, 0, sizeof(init));

    init.advdata.name_type          = BLE_ADVDATA_FULL_NAME;
    init.advdata.include_appearance = false;
    init.advdata.flags              = BLE_GAP_ADV_FLAGS_LE_ONLY_GENERAL_DISC_MODE;

    init.srdata.uuids_complete.uuid_cnt = sizeof(m_adv_uuids) / sizeof(m_adv_uuids[0]);
    init.srdata.uuids_complete.p_uuids  = m_adv_uuids;

    init.config.ble_adv_fast_enabled  = true;
    init.config.ble_adv_fast_interval = APP_ADV_FAST_INTERVAL;
    init.config.ble_adv_fast_timeout  = APP_ADV_FAST_DURATION;
		init.config.ble_adv_slow_enabled  = true;
		init.config.ble_adv_slow_interval = APP_ADV_SLOW_INTERVAL;
		init.config.ble_adv_slow_timeout	= APP_ADV_SLOW_DURATION;
    init.evt_handler = on_adv_evt;

    err_code = ble_advertising_init(&m_advertising, &init);
    APP_ERROR_CHECK(err_code);

    ble_advertising_conn_cfg_tag_set(&m_advertising, APP_BLE_CONN_CFG_TAG);
}


/**@brief Function for initializing buttons and leds.
 *3
 * @param[out] p_erase_bonds  Will be true if the clear bonding button was pressed to wake the application up.
 */
static void buttons_leds_init(bool * p_erase_bonds)
{
    bsp_event_t startup_event;

    //uint32_t err_code = bsp_init(BSP_INIT_LEDS | BSP_INIT_BUTTONS, bsp_event_handler);
		uint32_t err_code = bsp_init(BSP_INIT_LEDS, bsp_event_handler);
    APP_ERROR_CHECK(err_code);

    err_code = bsp_btn_ble_init(NULL, &startup_event);
    APP_ERROR_CHECK(err_code);

    *p_erase_bonds = (startup_event == BSP_EVENT_CLEAR_BONDING_DATA);
}


/**@brief Function for initializing the nrf log module.
 */
static void log_init(void)
{
    ret_code_t err_code = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(err_code);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}


/**@brief Function for initializing power management.
 */
static void power_management_init(void)
{
    ret_code_t err_code;
    err_code = nrf_pwr_mgmt_init();
    APP_ERROR_CHECK(err_code);
}


/**@brief Function for handling the idle state (main loop).
 *
 * @details If there is no pending log operation, then sleep until next the next event occurs.
 */
static void idle_state_handle(void)
{
    if (NRF_LOG_PROCESS() == false)
    {
        nrf_pwr_mgmt_run();
    }
}


/**@brief Function for starting advertising.
 */
static void advertising_start(void)
{
    uint32_t err_code = ble_advertising_start(&m_advertising, BLE_ADV_MODE_FAST);
    APP_ERROR_CHECK(err_code);
}

//buffer_t m_buf;
//void ble_data_send_with_queue(void)
//{
//	uint32_t err_code;
//	uint16_t length = 0;
//	static bool retry = false;
//	
//	if (retry)
//	{
//		length = m_buf.length;
//		err_code = ble_nus_data_send(&m_nus, m_buf.p_data, &length, m_conn_handle);
//		//NRF_LOG_INFO("Data2: %d", m_buf.p_data[0]);
//		if ( (err_code != NRF_ERROR_INVALID_STATE) && (err_code != NRF_ERROR_RESOURCES) &&
//				 (err_code != NRF_ERROR_NOT_FOUND) )
//		{
//				APP_ERROR_CHECK(err_code);
//		}
//		if (err_code == NRF_SUCCESS)
//		{
//			m_len_sent += length;
//			retry = false;
//		}
//	}
//	
//	while (!nrf_queue_is_empty(&m_buf_queue) && !retry)
//	{		

//		err_code = nrf_queue_pop(&m_buf_queue, &m_buf);
//		APP_ERROR_CHECK(err_code);		
//		length = m_buf.length;
//					
//		err_code = ble_nus_data_send(&m_nus, m_buf.p_data, &length, m_conn_handle);
//		//NRF_LOG_INFO("Data: %d", m_buf.p_data[0]);
//		if ( (err_code != NRF_ERROR_INVALID_STATE) && (err_code != NRF_ERROR_RESOURCES) &&
//				 (err_code != NRF_ERROR_NOT_FOUND) )
//		{
//				APP_ERROR_CHECK(err_code);
//		}
//		if (err_code == NRF_SUCCESS)
//		{
//			m_len_sent += length;
//			retry = false;
//		}
//		else
//		{
//			retry = true;
//			break;
//		}
//	}			
//}

//static void throughput_timer_handler(void * p_context)
//{
//#ifndef APP_QUEUE	
//	//the snippet used to test data throughput only. no queue is involved
//	ret_code_t err_code;
//	uint16_t length;
//	m_cnt_7ms++;	
//	//sending code lines
//	length = m_ble_nus_max_data_len;	
//	do
//	{					
//		err_code = ble_nus_data_send(&m_nus, m_data_array, &length, m_conn_handle);
//		if ( (err_code != NRF_ERROR_INVALID_STATE) && (err_code != NRF_ERROR_RESOURCES) &&
//				 (err_code != NRF_ERROR_NOT_FOUND) )
//		{
//				APP_ERROR_CHECK(err_code);
//		}
//		if (err_code == NRF_SUCCESS)
//		{
//			m_len_sent += length; 	
//			m_data_array[0]++;
//			m_data_array[length-1]++;	
//		}
//	} while (err_code == NRF_SUCCESS);

//	//calculate speed every 1 second
//	if (m_cnt_7ms == 143)
//	{
//		NRF_LOG_INFO("==**Speed: %d B/s**==", m_len_sent);
//		m_cnt_7ms = 0;
//		m_len_sent = 0;
//		m_data_array[0] = 0;
//		m_data_array[length-1] = 0;
//	}	
//	//NRF_LOG_INFO("PacketNo.: %d == Time: %d *7ms", m_data_array[0], m_cnt_7ms);	
//#else
//	//the snippet simulate a real application scenairo. Queue is involved.
//	ret_code_t err_code1, err_code2;	
//	buffer_t buf;
//	static uint8_t val = 0;
//	//produce the data irregard of BLE activity
//	m_data_array[(m_cnt_7ms%10)*420] = val++;
//	m_data_array[(m_cnt_7ms%10)*420+210] = val++;
//	
//	//put the data into a queue to cache them
//	buf.p_data = &m_data_array[(m_cnt_7ms%10)*420];
//	buf.length = MIN(m_ble_nus_max_data_len,210);
//	err_code1 = nrf_queue_push(&m_buf_queue, &buf);
//	//APP_ERROR_CHECK(err_code1); //it may return NRF_ERROR_NO_MEM. we skip this error
//	
//	buf.p_data = &m_data_array[(m_cnt_7ms%10)*420+210];
//	buf.length = MIN(m_ble_nus_max_data_len,210);
//	err_code2 = nrf_queue_push(&m_buf_queue, &buf);
//	//APP_ERROR_CHECK(err_code2);	//it may return NRF_ERROR_NO_MEM. we skip this error
//	
//	ble_data_send_with_queue();
//	
//	if(err_code1 == NRF_ERROR_NO_MEM || err_code2 == NRF_ERROR_NO_MEM)
//	{
//		NRF_LOG_INFO("Drop");	
//	}
//	
//	m_cnt_7ms++;	
//	//calculate speed every 1 second
//	if (m_cnt_7ms == 143)
//	{
//		NRF_LOG_INFO("==**Speed: %d B/s**==", m_len_sent);
//		m_cnt_7ms = 0;
//		m_len_sent = 0;
//	}	
//	//NRF_LOG_INFO("Time: %d *7ms", m_cnt_7ms);		
//	
//#endif	
//}


//void throughput_test()
//{
//	ret_code_t err_code;
//	err_code = app_timer_create(&m_timer_speed, APP_TIMER_MODE_REPEATED, throughput_timer_handler);
//	APP_ERROR_CHECK(err_code);

//#if 0
//	  ble_opt_t  opt;
//    memset(&opt, 0x00, sizeof(opt));
//    opt.common_opt.conn_evt_ext.enable = true;
//    err_code = sd_ble_opt_set(BLE_COMMON_OPT_CONN_EVT_EXT, &opt);
//    APP_ERROR_CHECK(err_code);
//#endif	
//	
//}

static void wdt_timer_handler(void * p_context)
{
	static uint8_t wdt_sec=0;
	wdt_sec++;
	nrfx_wdt_channel_feed(my_channel_id);
	nrf_gpio_pin_toggle(TPL5010_DONE);
	//NRF_LOG_INFO("FEED DOG:%d",wdt_sec);
}

//static void advertising_timer_handler(void * p_context)
//{
//	// 创造一个间隔发广播的中断函数
//	NRF_LOG_INFO("ADVERTISING START");
//	advertising_start();
//}

void app_timer_config()
{
		ret_code_t err_code;
	  err_code = app_timer_create(&wdt_timer_speed, APP_TIMER_MODE_REPEATED, wdt_timer_handler);
	  APP_ERROR_CHECK(err_code);
		APP_ERROR_CHECK(app_timer_start(wdt_timer_speed, APP_TIMER_TICKS(1000),NULL));
//	err_code = app_timer_create(&advertising_timer, APP_TIMER_MODE_REPEATED, advertising_timer_handler);
//	APP_ERROR_CHECK(err_code);
//	APP_ERROR_CHECK(app_timer_start(wdt_timer_speed, APP_TIMER_TICKS(30000),NULL));
}

//定时器事件回调函数
void my_timer_event_handler(nrf_timer_event_t event_type,void* p_context)
{
	switch(event_type)
	{
		case NRF_TIMER_EVENT_COMPARE0:
			nrf_gpio_pin_toggle(LED_3);
			break;
		default:
			break;
	}
}
//定时器配置函数
void timer_config()
{
	uint32_t err_code=NRF_SUCCESS;
	//uint32_t time_ms=1;
	uint32_t time_us=500;
	uint32_t ticks;

	nrfx_timer_config_t my_timer_config = NRFX_TIMER_DEFAULT_CONFIG; // 先使用默认配置，然后更改
	my_timer_config.frequency = NRF_TIMER_FREQ_1MHz; // 在头文件nrf_timer.h中
	err_code=nrfx_timer_init(&MY_TIMER,&my_timer_config,my_timer_event_handler); //无论什么模式，都要给出事件回调函数，即使用不到，也要定义
	APP_ERROR_CHECK(err_code);
	
	//ticks=nrfx_timer_ms_to_ticks(&MY_TIMER,time_ms);
	ticks=nrfx_timer_us_to_ticks(&MY_TIMER,time_us);
	nrfx_timer_extended_compare(&MY_TIMER,NRF_TIMER_CC_CHANNEL0,ticks,NRF_TIMER_SHORT_COMPARE0_CLEAR_MASK,false); //最后一项是中断，不开启中断，则没有事件回调
	
	//nrfx_timer_enable(&MY_TIMER);
}

void ave(short* buffer)
{
	int tmp1=0;
	int tmp2=0;
	int tmp3=0;
	int tmp4=0;
	for(int i=0;i<SAMPLE_BUFFER_DOTS;i++)
	{
		tmp1 += buffer[i*4];
		tmp2 += buffer[i*4+1];
		tmp3 += buffer[i*4+2];
		tmp4 += buffer[i*4+3];
	}
	ave_data[hc_num][0] =tmp1/SAMPLE_BUFFER_DOTS;
	ave_data[hc_num][1] =tmp2/SAMPLE_BUFFER_DOTS;
	ave_data[hc_num][2] =tmp3/SAMPLE_BUFFER_DOTS;
	ave_data[hc_num][3] =tmp4/SAMPLE_BUFFER_DOTS;
	if(hc_num < HC)
		hc_num++;
	else
		hc_num=0;
}

void rms(short* buffer)
{
	int tmp1=0;
	int tmp2=0;
	int tmp3=0;
	int tmp4=0;
	int tmp5=0;
	int tmp6=0;
	int tmp7=0;
	int tmp8=0;
	for(int i=0;i<SAMPLE_BUFFER_DOTS;i++)
	{
		tmp1 += buffer[i*4]*buffer[i*4];
		tmp2 += buffer[i*4+1]*buffer[i*4+1];
		tmp3 += buffer[i*4+2]*buffer[i*4+2];
		tmp4 += buffer[i*4+3]*buffer[i*4+3];
		tmp5 += buffer[i*4];
		tmp6 += buffer[i*4+1];
		tmp7 += buffer[i*4+2];
		tmp8 += buffer[i*4+3];
	}
	// 或者直接取平方均值的9-24位（4096=2^12）(计算得500以下的有效值也才相差0.2)
//	double temp1 =tmp1/SAMPLE_BUFFER_DOTS;
//	double temp2 =tmp2/SAMPLE_BUFFER_DOTS;
//	double temp3 =tmp3/SAMPLE_BUFFER_DOTS;
//	double temp4 =tmp4/SAMPLE_BUFFER_DOTS;
	double temp1 =sqrt(tmp1-tmp5*tmp5);
	double temp2 =sqrt(tmp2-tmp6*tmp6);
	double temp3 =sqrt(tmp3-tmp7*tmp7);
	double temp4 =sqrt(tmp4-tmp8*tmp8);
	ave_data[hc_num][0] =(uint16_t)temp1;
	ave_data[hc_num][1] =(uint16_t)temp2;
	ave_data[hc_num][2] =(uint16_t)temp3;
	ave_data[hc_num][3] =(uint16_t)temp4;
//	ave_data[hc_num][0] =(uint16_t)(tmp1-tmp5*tmp5);
//	ave_data[hc_num][1] =(uint16_t)(tmp2-tmp6*tmp6);
//	ave_data[hc_num][2] =(uint16_t)(tmp3-tmp7*tmp7);
//	ave_data[hc_num][3] =(uint16_t)(tmp4-tmp8*tmp8);
	if(hc_num < HC)
		hc_num++;
	else
		hc_num=0;
}

void saadc_callback(nrf_drv_saadc_evt_t const * p_event)
{
	ret_code_t err_code=NRF_SUCCESS;
	
	if(p_event->type==NRFX_SAADC_EVT_DONE)
	{
		//设置缓存，为下一次采样准备，具体我也不懂
		APP_ERROR_CHECK(nrfx_saadc_buffer_convert(p_event->data.done.p_buffer,SAMPLE_BUFFER_LEN));
		
		//NRF_LOG_INFO("SAADC");
		ave((short*)p_event->data.done.p_buffer); //直流磁场API
		//rms((short*)p_event->data.done.p_buffer);
		if(hc_num==0){
			m_adc_evt_counter++;
			length=MIN(m_ble_nus_max_data_len,HC*CHANNEL*2);
			err_code = ble_nus_data_send(&m_nus, (uint8_t*)ave_data, &length, m_conn_handle);
			if ( (err_code != NRF_ERROR_INVALID_STATE) && (err_code != NRF_ERROR_RESOURCES) &&
				 (err_code != NRF_ERROR_NOT_FOUND) )
			{
					APP_ERROR_CHECK(err_code);
			}
			if (err_code == NRF_SUCCESS)
			{
				//NRF_LOG_INFO("%d:SUCCESS",m_adc_evt_counter);
				m_adv_success_send++;
			}
			else
			{
				//NRF_LOG_INFO("%d:DROP",m_adc_evt_counter);
			}
			if(m_adc_evt_counter==80)
			{
				//NRF_LOG_INFO("RATE: %d/%d",m_adv_success_send,m_adc_evt_counter);
				m_adv_success_send=0;
				m_adc_evt_counter=0;
			}
		}
	}
}
	
void adc_config()
{
	ret_code_t err_code=NRF_SUCCESS;
	
	//ADC通道配置结构体
	//nrf_saadc_channel_config_t channel_config = NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_DIFFERENTIAL(NRF_SAADC_INPUT_AIN1,NRF_SAADC_INPUT_AIN0);
	nrf_saadc_channel_config_t channel_config = NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_DIFFERENTIAL(NRF_SAADC_INPUT_VDD,NRF_SAADC_INPUT_AIN3);
	nrf_saadc_channel_config_t channel_config2 = NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_DIFFERENTIAL(NRF_SAADC_INPUT_AIN3,NRF_SAADC_INPUT_AIN2);
	nrf_saadc_channel_config_t channel_config3 = NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_DIFFERENTIAL(NRF_SAADC_INPUT_AIN5,NRF_SAADC_INPUT_AIN4);
	//nrf_saadc_channel_config_t channel_config4 = NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_DIFFERENTIAL(NRF_SAADC_INPUT_AIN6,NRF_SAADC_INPUT_AIN7);
	nrf_saadc_channel_config_t channel_config4 = NRFX_SAADC_DEFAULT_CHANNEL_CONFIG_SE(NRF_SAADC_INPUT_VDD);
	//初始化SAADC，注册事件回调函数 
	err_code=nrf_drv_saadc_init(NULL,saadc_callback);
	APP_ERROR_CHECK(err_code);
	//初始化SAADC通道0
	err_code=nrfx_saadc_channel_init(0,&channel_config);
	APP_ERROR_CHECK(err_code);
	//初始化SAADC通道1
	err_code=nrfx_saadc_channel_init(1,&channel_config2);
	APP_ERROR_CHECK(err_code);	
		//初始化SAADC通道2
	err_code=nrfx_saadc_channel_init(2,&channel_config3);
	APP_ERROR_CHECK(err_code);	
		//初始化SAADC通道3
	err_code=nrfx_saadc_channel_init(3,&channel_config4);
	APP_ERROR_CHECK(err_code);	
	//注册双缓存
	err_code=nrfx_saadc_buffer_convert(m_buffer_pool[0],SAMPLE_BUFFER_LEN);
	APP_ERROR_CHECK(err_code);
	err_code=nrfx_saadc_buffer_convert(m_buffer_pool[1],SAMPLE_BUFFER_LEN);
	APP_ERROR_CHECK(err_code);
	
}

void feed_tpl5010()
{
		nrf_gpio_pin_set(TPL5010_DONE);
		nrf_delay_ms(50);
		nrf_gpio_pin_clear(TPL5010_DONE);
}

void tpl5010_handler(nrf_drv_gpiote_pin_t pin,nrf_gpiote_polarity_t action)
{
	if(pin==TPL5010_WAKE)
	{
		feed_tpl5010();
	}
}

//TPL5010喂狗
void tpl5010_gpiote_config()
{
	ret_code_t err_code;

	if (!nrf_drv_gpiote_is_init())
  {
			err_code = nrf_drv_gpiote_init();
			APP_ERROR_CHECK(err_code);
			nrf_drv_gpiote_in_config_t in_config_hitlo = GPIOTE_CONFIG_IN_SENSE_HITOLO(false);
			in_config_hitlo.pull = NRF_GPIO_PIN_PULLUP;
			err_code = nrf_drv_gpiote_in_init(TPL5010_WAKE,&in_config_hitlo,tpl5010_handler);
			APP_ERROR_CHECK(err_code);
			nrf_drv_gpiote_in_event_enable(TPL5010_WAKE,true);
  }
	else
	{
			//NRF_LOG_INFO("GPIOTE HAS BEENINITIALIZED!");
	}
	
	
}

void ppi_config()
{
	uint32_t err_code=NRF_SUCCESS;
	
	//初始化PPI模块
	err_code=nrf_drv_ppi_init();
	APP_ERROR_CHECK(err_code);
	
	//申请PPI通道
	err_code = nrfx_ppi_channel_alloc(&my_ppi_channel);
	APP_ERROR_CHECK(err_code);
	
	//设置通道的TEP和EEP
	err_code = nrfx_ppi_channel_assign(my_ppi_channel,nrfx_timer_compare_event_address_get(&MY_TIMER,NRF_TIMER_CC_CHANNEL0),nrfx_saadc_sample_task_get());
	APP_ERROR_CHECK(err_code);
	
	
	//设置次级任务端点
	//err_code = nrfx_ppi_channel_fork_assign(my_ppi_channel,nrfx_gpiote_out_task_addr_get(LED_4));
	//APP_ERROR_CHECK(err_code);
	
//	//使能PPI通道
//	err_code = nrfx_ppi_channel_enable(my_ppi_channel);
//	APP_ERROR_CHECK(err_code);
	
	//PPI是硬件活动，与中断/事件回调无关！不冲突！(GPIOTE会使得回调函数里对GPIOTE端口的GPIO操作失败)
}

void sensor_init()
{
	if(sensor_state==0)
	{
		NRF_LOG_INFO("SENSOR_INIT");
		adc_config();	
		timer_config();
		ppi_config();
		sensor_state=1;
	}
}

void sensor_uninit()
{
	if(sensor_state==1)
	{
		NRF_LOG_INFO("SENSOR_UNINIT");
		nrf_drv_ppi_uninit();
		nrfx_timer_uninit(&MY_TIMER);
		nrfx_saadc_uninit();
		sensor_state=0;
	}
}

//WDT 中断只有2个32.768kHz的时钟周期，之后系统复位
void wdt_event_handler(void)
{
	bsp_board_leds_on();
}

void wdt_config()
{
	uint32_t err_code=NRF_SUCCESS;
	
	// 喂狗设置，默认2000ms喂一次
	nrfx_wdt_config_t config = NRFX_WDT_DEAFULT_CONFIG;
	err_code=nrfx_wdt_init(&config,wdt_event_handler);
	APP_ERROR_CHECK(err_code);
	
	// 申请喂狗通道
	err_code = nrfx_wdt_channel_alloc(&my_channel_id);
	APP_ERROR_CHECK(err_code);
	
	nrf_gpio_cfg_output(TPL5010_DONE);
	nrf_gpio_pin_clear(TPL5010_DONE);
	
	nrf_gpio_cfg_output(CONTROL_VCC);
	nrf_gpio_pin_set(CONTROL_VCC);
	
	//开始计数，不可终止，所以该初始化函数最好最后执行
	nrfx_wdt_enable();
}
/**@brief Application main function.
 */
int main(void)
{
    bool erase_bonds;
		
		NRF_POWER->DCDCEN = 1;
	
		//nrf_delay_ms(1000);
    //Initialize.
    //uart_init();
    log_init();
    timers_init();
    buttons_leds_init(&erase_bonds); //偷偷的对button进行了gpiote port初始化
	
		//feed_tpl5010();
	
    power_management_init();
    ble_stack_init();
    gap_params_init();
    gatt_init();
    services_init();
    advertising_init();
    conn_params_init();
		
    // Start execution.
    //printf("\r\nUART started.\r\n");
    NRF_LOG_INFO("Debug logging for UART over RTT started.");	
		
		app_timer_config();
		wdt_config();
	
		advertising_start();
		//throughput_test();



//		sensor_init();
//		//使能PPI通道
//		nrfx_timer_enable(&MY_TIMER);
//		APP_ERROR_CHECK(nrfx_ppi_channel_enable(my_ppi_channel));		
		
		
		
		//sensor_uninit();

    // Enter main loop.
    for (;;)
    {
        idle_state_handle();
    }
}


/**
 * @}
 */

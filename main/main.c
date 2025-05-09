/*	CAN Monitor example

	This example code is in the Public Domain (or CC0 licensed, at your option.)

	Unless required by applicable law or agreed to in writing, this
	software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	CONDITIONS OF ANY KIND, either express or implied.
*/

#include <stdio.h>
#include <inttypes.h>
#include <string.h>
#include <string.h>
#include <sys/socket.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_vfs.h"
#include "nvs_flash.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "driver/twai.h" // Update from V4.2
#include "tinyusb.h"
#include "tusb_cdc_acm.h"
#include "cJSON.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#define RX_BUF_SIZE 128

#if CONFIG_UART_INTERFACE
static const char *UART_TX_TASK_TAG = "UART_TX_TASK";
static const char *UART_RX_TASK_TAG = "UART_RX_TASK";
#else
static const char *TUSB_TX_TASK_TAG = "TUSB_TX_TASK";
static const char *TUSB_RX_TASK_TAG = "TUSB_RX_TASK";
#endif
static const char *TWAI_TX_TASK_TAG = "TWAI_TX_TASK";
static const char *TWAI_RX_TASK_TAG = "TWAI_RX_TASK";
static const char *CONTROL_TASK_TAG = "CONTROL_TASK";

typedef struct {
	int		bytes;
	uint8_t	data[RX_BUF_SIZE];
} UART_t;

typedef struct {
	int			counter;
	int			enable;
	uint32_t	id[100];
} CONFIG_t;

static QueueHandle_t xQueue_uart_rx;
static QueueHandle_t xQueue_uart_tx;
static QueueHandle_t xQueue_twai_rx;
static QueueHandle_t xQueue_twai_tx;
static EventGroupHandle_t xEventGroup;
static SemaphoreHandle_t ctrl_task_sem;

#define TWAI_START_BIT		BIT0
#define TWAI_STATUS_BIT		BIT4


static int g_recv_error = 0;
static int g_send_error = 0;

#if CONFIG_UART_INTERFACE
void uart_init(void) {
	const uart_config_t uart_config = {
		.baud_rate = 115200,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
#if (ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0))
		.source_clk = UART_SCLK_DEFAULT,
#else
		.source_clk = UART_SCLK_APB,
#endif
	};
	// We won't use a buffer for sending data.
	ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0));
	ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));
	//uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, CONFIG_UART_TX_GPIO, CONFIG_UART_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
}

static void uart_tx_task(void *arg)
{
	esp_log_level_set(UART_TX_TASK_TAG, ESP_LOG_INFO);
	ESP_LOGI(UART_TX_TASK_TAG, "Start");

	UART_t uartBuf;

	while (1) {
		//Waiting for UART transmit event.
		if (xQueueReceive(xQueue_uart_tx, &uartBuf, portMAX_DELAY) == pdTRUE) {
			ESP_LOGI(UART_TX_TASK_TAG, "uartBuf.bytes=%d", uartBuf.bytes);
			ESP_LOG_BUFFER_HEXDUMP(UART_TX_TASK_TAG, uartBuf.data, uartBuf.bytes, ESP_LOG_INFO);
			int txBytes = uart_write_bytes(UART_NUM_1, uartBuf.data, uartBuf.bytes);
			ESP_LOGI(UART_TX_TASK_TAG, "txBytes=%d", txBytes);
		}
	}
}

static void uart_rx_task(void *arg)
{
	esp_log_level_set(UART_RX_TASK_TAG, ESP_LOG_INFO);
	ESP_LOGI(UART_RX_TASK_TAG, "Start");
	uint8_t* data = (uint8_t*) malloc(RX_BUF_SIZE+1);
	if (data == NULL) {
		ESP_LOGE(UART_RX_TASK_TAG, "malloc Fail");
		vTaskDelete(NULL);
	}
	UART_t uartBuf;

	while (1) {
		const int rxBytes = uart_read_bytes(UART_NUM_1, data, RX_BUF_SIZE, 1000 / portTICK_PERIOD_MS);
		// There is some data in rx buffer
		if (rxBytes > 0) {
			data[rxBytes] = 0;
			//ESP_LOGI(UART_RX_TASK_TAG, "Read %d bytes: '%s'", rxBytes, data);
			ESP_LOGI(UART_RX_TASK_TAG, "Read %d", rxBytes);
			ESP_LOG_BUFFER_HEXDUMP(UART_RX_TASK_TAG, data, rxBytes, ESP_LOG_INFO);
			uartBuf.bytes = rxBytes;
			for (int i=0;i<rxBytes;i++) {
				uartBuf.data[i] = data[i];
			}
			if (xQueueSend(xQueue_uart_rx, &uartBuf, portMAX_DELAY) != pdPASS) {
				ESP_LOGE(UART_RX_TASK_TAG, "xQueueSend Fail");
			}


		// There is no data in rx buufer
		} else {
			//ESP_LOGI(UART_RX_TASK_TAG, "Read %d", rxBytes);
		}
	}
	free(data);
}
#endif

#if CONFIG_USB_INTERFACE
void tinyusb_cdc_rx_callback(int itf, cdcacm_event_t *event)
{
	/* initialization */
	size_t rxBytes = 0;
	ESP_LOGD(TUSB_RX_TASK_TAG, "CONFIG_TINYUSB_CDC_RX_BUFSIZE=%d", CONFIG_TINYUSB_CDC_RX_BUFSIZE);

	/* read */
	uint8_t buf[CONFIG_TINYUSB_CDC_RX_BUFSIZE];
	esp_err_t ret = tinyusb_cdcacm_read(itf, buf, CONFIG_TINYUSB_CDC_RX_BUFSIZE, &rxBytes);
	if (ret == ESP_OK) {
		ESP_LOGD(TUSB_RX_TASK_TAG, "Data from channel=%d rxBytes=%d", itf, rxBytes);
		ESP_LOG_BUFFER_HEXDUMP(TUSB_RX_TASK_TAG, buf, rxBytes, ESP_LOG_INFO);
		UART_t uartBuf;
		uartBuf.bytes = rxBytes;
		for (int i=0;i<rxBytes;i++) {
			uartBuf.data[i] = buf[i];
		}
		if (xQueueSend(xQueue_uart_rx, &uartBuf, portMAX_DELAY) != pdPASS) {
			ESP_LOGE(TUSB_RX_TASK_TAG, "xQueueSend Fail");
		}
	} else {
		ESP_LOGE(TUSB_RX_TASK_TAG, "tinyusb_cdcacm_read error");
	}
}

void usb_init(void) {
	const tinyusb_config_t tusb_cfg = {
		.device_descriptor = NULL,
		.string_descriptor = NULL,
		.external_phy = false,
		.configuration_descriptor = NULL,
	};
	ESP_ERROR_CHECK(tinyusb_driver_install(&tusb_cfg));

	tinyusb_config_cdcacm_t acm_cfg = {
		.usb_dev = TINYUSB_USBDEV_0,
		.cdc_port = TINYUSB_CDC_ACM_0,
		.rx_unread_buf_sz = 64,
		.callback_rx = &tinyusb_cdc_rx_callback, // the first way to register a callback
		.callback_rx_wanted_char = NULL,
		//.callback_line_state_changed = &tinyusb_cdc_line_state_changed_callback,
		.callback_line_coding_changed = NULL
	};
	ESP_ERROR_CHECK(tusb_cdc_acm_init(&acm_cfg));
}

static void tusb_tx_task(void *arg)
{
	esp_log_level_set(TUSB_TX_TASK_TAG, ESP_LOG_INFO);
	ESP_LOGI(TUSB_TX_TASK_TAG, "Start");

	UART_t uartBuf;

	while (1) {
		//Waiting for UART transmit event.
		if (xQueueReceive(xQueue_uart_tx, &uartBuf, portMAX_DELAY) == pdTRUE) {
			ESP_LOGI(TUSB_TX_TASK_TAG, "uartBuf.bytes=%d", uartBuf.bytes);
			ESP_LOG_BUFFER_HEXDUMP(TUSB_TX_TASK_TAG, uartBuf.data, uartBuf.bytes, ESP_LOG_INFO);
			tinyusb_cdcacm_write_queue(TINYUSB_CDC_ACM_0, uartBuf.data, uartBuf.bytes);
			tinyusb_cdcacm_write_flush(TINYUSB_CDC_ACM_0, 0);
		}
	}
}
#endif

static void twai_receive_task(void *arg)
{
	esp_log_level_set(TWAI_RX_TASK_TAG, ESP_LOG_INFO);
	ESP_LOGI(TWAI_RX_TASK_TAG, "Start");

	twai_message_t rx_msg;
	
	while (1) {
		EventBits_t eventBit = xEventGroupGetBits(xEventGroup);
		ESP_LOGD(TWAI_RX_TASK_TAG,"eventBit=0x%"PRIx32, eventBit);
		if ( (eventBit & TWAI_START_BIT) != TWAI_START_BIT) {
			ESP_LOGD(TWAI_RX_TASK_TAG,"eventBit not set");
			xEventGroupClearBits( xEventGroup, TWAI_STATUS_BIT );
			vTaskDelay(1);
		} else {
			xEventGroupSetBits( xEventGroup, TWAI_STATUS_BIT );
			ESP_LOGD(TWAI_RX_TASK_TAG,"twai_receive");
			//twai_message_t rx_msg;
			xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);
			esp_err_t ret = twai_receive(&rx_msg, pdMS_TO_TICKS(1000));
			xSemaphoreGive(ctrl_task_sem);
			if (ret == ESP_OK) {
				ESP_LOGI(TWAI_RX_TASK_TAG,"twai_receive identifier=0x%"PRIx32" flags=0x%"PRIx32"-0x%x-0x%x data_length_code=%d",
					rx_msg.identifier, rx_msg.flags, rx_msg.extd, rx_msg.rtr, rx_msg.data_length_code);
				if (xQueueSend(xQueue_twai_rx, &rx_msg, portMAX_DELAY) != pdPASS) {
					ESP_LOGE(TWAI_RX_TASK_TAG, "xQueueSend Fail");
				}
			} else if (ret != ESP_ERR_TIMEOUT) {
				g_recv_error++;
				ESP_LOGE(TWAI_RX_TASK_TAG, "twai_receive Fail %s", esp_err_to_name(ret));
			}
		}
	}

}

static void twai_transmit_task(void *arg)
{
	esp_log_level_set(TWAI_TX_TASK_TAG, ESP_LOG_INFO);
	ESP_LOGI(TWAI_TX_TASK_TAG, "Start");

	twai_message_t tx_msg;

	while (1) {
		//Waiting for TWAI transmit event.
		if (xQueueReceive(xQueue_twai_tx, &tx_msg, portMAX_DELAY) == pdTRUE) {
			ESP_LOGI(TWAI_TX_TASK_TAG, "tx_msg.identifier=[0x%"PRIx32"]", tx_msg.identifier);
			//ESP_LOG_BUFFER_HEXDUMP(TWAI_TX_TASK_TAG, tx_msg.data, tx_msg.data_length_code, ESP_LOG_INFO);
			xSemaphoreTake(ctrl_task_sem, portMAX_DELAY);
			esp_err_t ret = twai_transmit(&tx_msg, pdMS_TO_TICKS(1000));
			xSemaphoreGive(ctrl_task_sem);
			if (ret == ESP_OK) {
				ESP_LOGI(TWAI_TX_TASK_TAG, "twai_transmit success");
			} else {
				g_send_error++;
				ESP_LOGE(TWAI_TX_TASK_TAG, "twai_transmit Fail %s", esp_err_to_name(ret));
			}
		}
	}

}

bool isValidId(CONFIG_t config, uint32_t identifier) 
{
	if (config.counter == 0) return true;

	for(int i=0;i<config.counter;i++) {
		if (config.enable == 0x10 && config.id[i] == identifier) return true;
		if (config.enable == 0x11 && config.id[i] == identifier) return false;
	}
	if (config.enable == 0x10) return false;
	if (config.enable == 0x11) return true;
	return true;
}


uint8_t calcCRC(uint8_t * buffer, int length)
{
	uint32_t crc = 0;
	for(int i=0;i<length;i++) {
		crc = crc + buffer[i];
	}
	ESP_LOGD(CONTROL_TASK_TAG, "crc=0x%"PRIx32, crc);
	return (crc & 0xFF);
}

static void control_task(void *arg)
{
	esp_log_level_set(CONTROL_TASK_TAG, ESP_LOG_INFO);
	ESP_LOGI(CONTROL_TASK_TAG, "Start");

	int driverInstalled = 0;
	UART_t uartBuf;
	CONFIG_t config;
	config.counter = 0;
	twai_message_t twaiBuf;
	

	while(1) {
		//Waiting for UART receive event.
		if (xQueueReceive(xQueue_uart_rx, &uartBuf, 0) == pdTRUE) {
			ESP_LOGI(CONTROL_TASK_TAG, "uartBuf.bytes=%d", uartBuf.bytes);
			ESP_LOGI(CONTROL_TASK_TAG, "uartBuf.data[0]=0x%x", uartBuf.data[0]);
			//ESP_LOG_BUFFER_HEXDUMP(CONTROL_TASK_TAG, uartBuf.data, uartBuf.bytes, ESP_LOG_INFO);
			if (uartBuf.data[0] != 0xAA) continue;
			
			uint8_t _cmd = uartBuf.data[1] >> 4;
			uint8_t _len = uartBuf.data[1] & 0xF;
			ESP_LOGI(CONTROL_TASK_TAG, "_cmd=[%x] _len==[%d]", _cmd, _len);

			//	Set and Start(Auto set bps)
			if (uartBuf.data[1] == 0x55 && uartBuf.data[2] == 0x12) {
				if (driverInstalled == 1) {
					// pause twai task
					xEventGroupClearBits( xEventGroup, TWAI_START_BIT );
					while(1) {
						EventBits_t eventBit = xEventGroupGetBits(xEventGroup);
						ESP_LOGD(CONTROL_TASK_TAG,"eventBit=0x%"PRIx32, eventBit);
						if ( (eventBit & TWAI_STATUS_BIT) != TWAI_STATUS_BIT) break;
						vTaskDelay(1);
					}
	
					//Stop and Uninstall TWAI driver
					ESP_ERROR_CHECK(twai_stop());
					ESP_LOGI(CONTROL_TASK_TAG, "Driver stopped");
					ESP_ERROR_CHECK(twai_driver_uninstall());
					ESP_LOGI(CONTROL_TASK_TAG, "Driver uninstalled");

				}

				// Set accept filter
				//static const twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
				twai_filter_config_t f_config;
				f_config.acceptance_code = 0;
				f_config.acceptance_mask = 0xFFFFFFFF;

				uint32_t acceptance_code;
				uint32_t acceptance_mask;
				if (uartBuf.data[4] == 0x01) {
					acceptance_code = ( (uartBuf.data[6] & 0x7) << 8) + uartBuf.data[5];
					acceptance_mask = ( (uartBuf.data[10] & 0x7) << 8) + uartBuf.data[9];
					ESP_LOGI(CONTROL_TASK_TAG, "Standard filter code=0x%03"PRIx32" mask=0x%03"PRIx32, acceptance_code, acceptance_mask);
					f_config.acceptance_code = (acceptance_code << 21);
					f_config.acceptance_mask = ~(acceptance_mask << 21);
				} else {
#if 0
					acceptance_code = (uartBuf.data[8] & 0x1F);
					acceptance_code = acceptance_code << 24;
					acceptance_mask = (uartBuf.data[12] & 0x1F);
					acceptance_mask = acceptance_mask << 24;
					ESP_LOGI(CONTROL_TASK_TAG, "Extended filter code=0x%08x mask=0x%08x",acceptance_code, acceptance_mask);
#endif
					acceptance_code = ((uartBuf.data[8] & 0x1F) << 24) + (uartBuf.data[7] << 16) + (uartBuf.data[6] << 8) + uartBuf.data[5];
					acceptance_mask = ((uartBuf.data[12] & 0x1F) << 24) + (uartBuf.data[11] << 16) + (uartBuf.data[10] << 8) + uartBuf.data[9];
					ESP_LOGI(CONTROL_TASK_TAG, "Extended filter code=0x%08"PRIx32" mask=0x%08"PRIx32, acceptance_code, acceptance_mask);
					f_config.acceptance_code = (acceptance_code << 3);
					f_config.acceptance_mask = ~(acceptance_mask << 3);
				}


#if 0
				// Only 0x100 Standard
				f_config.acceptance_code = (0x100 << 21);
				f_config.acceptance_mask = ~(0x1FF << 21);
				ESP_LOGI(CONTROL_TASK_TAG, "f_config.acceptance_code=%x f_config.acceptance_mask=%x",
						f_config.acceptance_code, f_config.acceptance_mask);

				// 0x100-0x107 Standard
				f_config.acceptance_code = (0x100 << 21);
				f_config.acceptance_mask = ~(0x1F8 << 21);

				// 0x108-0x10F Standard
				f_config.acceptance_code = (0x108 << 21);
				f_config.acceptance_mask = ~(0x1F8 << 21);

				// Only 0x10000008 Extended
				f_config.acceptance_code = (0x10000008 << 3);
				f_config.acceptance_mask = ~(0x1FFFFFFF << 3);

				// 0x10000000-0x1000007 Extended
				f_config.acceptance_code = (0x10000000 << 3);
				f_config.acceptance_mask = ~(0x1FFFFFF8 << 3);

				// 0x10000008-0x100000F Extended
				f_config.acceptance_code = (0x10000008 << 3);
				f_config.acceptance_mask = ~(0x1FFFFFF8 << 3);
#endif

				f_config.single_filter = true;

				//TWAI_TIMING_CONFIG_500KBITS()
				//{.brp = 8, .tseg_1 = 15, .tseg_2 = 4, .sjw = 3, .triple_sampling = false}
				static const twai_timing_config_t t_config1 = TWAI_TIMING_CONFIG_1MBITS();
				static const twai_timing_config_t t_config2 = TWAI_TIMING_CONFIG_800KBITS();
				static const twai_timing_config_t t_config3 = TWAI_TIMING_CONFIG_500KBITS();
				static const twai_timing_config_t t_config4 = TWAI_TIMING_CONFIG_250KBITS();
				static const twai_timing_config_t t_config5 = TWAI_TIMING_CONFIG_125KBITS();
				static const twai_timing_config_t t_config6 = TWAI_TIMING_CONFIG_100KBITS();
				static const twai_timing_config_t t_config7 = TWAI_TIMING_CONFIG_50KBITS();

				//static const twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(TX_GPIO_NUM, RX_GPIO_NUM, TWAI_MODE_NORMAL);
				static const twai_general_config_t g_config = TWAI_GENERAL_CONFIG_DEFAULT(CONFIG_CTX_GPIO, CONFIG_CRX_GPIO, TWAI_MODE_NORMAL);

				// Set Bit rate
				int driverWillInstall = 0;
				if (uartBuf.data[3] == 1) {
					ESP_LOGI(CONTROL_TASK_TAG,"Speed = 1Mbps");
					ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config1, &f_config));
					driverWillInstall = true;
				} else if (uartBuf.data[3] == 2) {
					ESP_LOGI(CONTROL_TASK_TAG,"Speed = 800Kbps");
					ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config2, &f_config));
					driverWillInstall = true;
				} else if (uartBuf.data[3] == 3) {
					ESP_LOGI(CONTROL_TASK_TAG,"Speed = 500Kbps");
					ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config3, &f_config));
					driverWillInstall = true;
				} else if (uartBuf.data[3] == 4) {
					ESP_LOGE(CONTROL_TASK_TAG,"Speed = 400Kbps");
					ESP_LOGE(CONTROL_TASK_TAG,"This speed is NOT supported");
				} else if (uartBuf.data[3] == 5) {
					ESP_LOGI(CONTROL_TASK_TAG,"Speed = 250Kbps");
					ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config4, &f_config));
					driverWillInstall = true;
				} else if (uartBuf.data[3] == 6) {
					ESP_LOGE(CONTROL_TASK_TAG,"Speed = 200Kbps");
					ESP_LOGE(CONTROL_TASK_TAG,"This speed is NOT supported");
				} else if (uartBuf.data[3] == 7) {
					ESP_LOGI(CONTROL_TASK_TAG,"Speed = 125Kbps");
					ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config5, &f_config));
					driverWillInstall = true;
				} else if (uartBuf.data[3] == 8) {
					ESP_LOGI(CONTROL_TASK_TAG,"Speed = 100Kbps");
					ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config6, &f_config));
					driverWillInstall = true;
				} else if (uartBuf.data[3] == 9) {
					ESP_LOGI(CONTROL_TASK_TAG,"Speed = 50Kbps");
					ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config7, &f_config));
					driverWillInstall = true;
				} else if (uartBuf.data[3] == 10) {
					ESP_LOGE(CONTROL_TASK_TAG,"Speed = 20Kbps");
					ESP_LOGE(CONTROL_TASK_TAG,"This speed is NOT supported");
				} else if (uartBuf.data[3] == 11) {
					ESP_LOGE(CONTROL_TASK_TAG,"Speed = 10Kbps");
					ESP_LOGE(CONTROL_TASK_TAG,"This speed is NOT supported");
				} else if (uartBuf.data[3] == 12) {
					ESP_LOGE(CONTROL_TASK_TAG,"Speed = 5Kbps");
					ESP_LOGE(CONTROL_TASK_TAG,"This speed is NOT supported");
				}

				//Install and start TWAI driver
				if (driverWillInstall) {
					ESP_LOGI(CONTROL_TASK_TAG, "Driver installed");
					ESP_ERROR_CHECK(twai_start());
					ESP_LOGI(CONTROL_TASK_TAG, "Driver started");

					// start twai task
					xEventGroupSetBits( xEventGroup, TWAI_START_BIT );
					driverInstalled = 1;
				} else {
					ESP_LOGE(CONTROL_TASK_TAG, "Driver not installed");
				}

			// Send Standard CAN Frame
			} else if (_cmd == 0xC || _cmd == 0xD) {
				twaiBuf.identifier = (uartBuf.data[3] << 8) + uartBuf.data[2];
				twaiBuf.extd = 0;
				twaiBuf.rtr = 0;
				if (_cmd == 0xD) {
					twaiBuf.rtr = 1;
				}
				twaiBuf.ss = 1;
				twaiBuf.self = 0;
				twaiBuf.dlc_non_comp = 0;
				twaiBuf.data_length_code = _len;
				for(int i=0;i<_len;i++) {
					twaiBuf.data[i] = uartBuf.data[4+i];
				}
				if (xQueueSend(xQueue_twai_tx, &twaiBuf, portMAX_DELAY) != pdPASS) {
					ESP_LOGE(CONTROL_TASK_TAG, "xQueueSend Fail");
				}

			// Send Extended CAN Frame
			} else if (_cmd == 0xE || _cmd == 0xF) {
				twaiBuf.identifier = (uartBuf.data[5] << 24) + (uartBuf.data[4] << 16) + (uartBuf.data[3] << 8) + uartBuf.data[2];
				twaiBuf.extd = 1;
				twaiBuf.rtr = 0;
				if (_cmd == 0xF) {
					twaiBuf.rtr = 1;
				}
				twaiBuf.ss = 1;
				twaiBuf.self = 0;
				twaiBuf.dlc_non_comp = 0;
				twaiBuf.data_length_code = _len;
				for(int i=0;i<_len;i++) {
					twaiBuf.data[i] = uartBuf.data[4+i];
				}
				if (xQueueSend(xQueue_twai_tx, &twaiBuf, portMAX_DELAY) != pdPASS) {
					ESP_LOGE(CONTROL_TASK_TAG, "xQueueSend Fail");
				}

			//	Configure the receive ID(Only upload configured ID)
			} else if (uartBuf.data[1] == 0x55 && uartBuf.data[2] == 0x10) {
				config.counter = uartBuf.data[3];
				config.enable = uartBuf.data[2];
				for(int i=0;i<config.counter;i++){
					config.id[i] = (uartBuf.data[i*4+7] << 24) + (uartBuf.data[i*4+6] << 16) + (uartBuf.data[i*4+5] << 8) + (uartBuf.data[i*4+4]);
					ESP_LOGI(CONTROL_TASK_TAG, "config.id[%d]=0x%"PRIx32, i, config.id[i]);
				}

			//	Configure the receive ID(Receive ID not uploaded)
			} else if (uartBuf.data[1] == 0x55 && uartBuf.data[2] == 0x11) {
				config.counter = uartBuf.data[3];
				config.enable = uartBuf.data[2];
				for(int i=0;i<config.counter;i++){
					config.id[i] = (uartBuf.data[i*4+7] << 24) + (uartBuf.data[i*4+6] << 16) + (uartBuf.data[i*4+5] << 8) + (uartBuf.data[i*4+4]);
					ESP_LOGI(CONTROL_TASK_TAG, "config.id[%d]=0x%"PRIx32, i, config.id[i]);
				}

			//	Monitor
			} else if (uartBuf.data[1] == 0x55 && uartBuf.data[2] == 0x04) {
				uartBuf.data[3] = g_recv_error;
				uartBuf.data[4] = g_send_error;
				if (uartBuf.data[3] != 0 || uartBuf.data[4] != 0) uartBuf.data[6] = 1;

#if 0
				uartBuf.data[3] = 10;
				uartBuf.data[4] = 20;
				uartBuf.data[6] = 1; // Error Flag
#endif

				uartBuf.data[19] = calcCRC(&uartBuf.data[2], 17);
				ESP_LOGI(CONTROL_TASK_TAG, "uartBuf.data[19]=0x%x", uartBuf.data[19]);
				if (xQueueSend(xQueue_uart_tx, &uartBuf, portMAX_DELAY) != pdPASS) {
					ESP_LOGE(CONTROL_TASK_TAG, "xQueueSend Fail");
				}

			}

		//Waiting for TWAI receive event.
		} else if (xQueueReceive(xQueue_twai_rx, &twaiBuf, 0) == pdTRUE) {
			ESP_LOGI(CONTROL_TASK_TAG,"twai_receive identifier=0x%"PRIx32" flags=0x%"PRIx32"-0x%x-0x%x data_length_code=%d",
				twaiBuf.identifier, twaiBuf.flags, twaiBuf.extd, twaiBuf.rtr, twaiBuf.data_length_code);
			//int ext = twaiBuf.flags & 0x01;
			//int rtr = twaiBuf.flags & 0x02;

			// Check identifier
			if (isValidId(config, twaiBuf.identifier) == false) continue;
			if (twaiBuf.extd == 0) {
				uartBuf.bytes = 13;
				memset(uartBuf.data, 0, uartBuf.bytes);
				uartBuf.data[0] = 0xAA;
				if (twaiBuf.rtr == 0) {
					uartBuf.data[1] = 0xC0 + twaiBuf.data_length_code;
				} else {
					uartBuf.data[1] = 0xD0 + twaiBuf.data_length_code;
				}
				uartBuf.data[2] = (twaiBuf.identifier & 0xFF);
				uartBuf.data[3] = (twaiBuf.identifier >> 8) & 0xFF;
				for(int i=0;i<twaiBuf.data_length_code;i++) {
					uartBuf.data[4+i] = twaiBuf.data[i];
				}
				uartBuf.data[12] = 0x55;
				if (xQueueSend(xQueue_uart_tx, &uartBuf, portMAX_DELAY) != pdPASS) {
					ESP_LOGE(CONTROL_TASK_TAG, "xQueueSend Fail");
				}
			} else {
				uartBuf.bytes = 15;
				memset(uartBuf.data, 0, uartBuf.bytes);
				uartBuf.data[0] = 0xAA;
				if (twaiBuf.rtr == 0) {
					uartBuf.data[1] = 0xE0 + twaiBuf.data_length_code;
				} else {
					uartBuf.data[1] = 0xF0 + twaiBuf.data_length_code;
				}
				uartBuf.data[2] = (twaiBuf.identifier & 0xFF);
				uartBuf.data[3] = (twaiBuf.identifier >> 8) & 0xFF;
				uartBuf.data[4] = (twaiBuf.identifier >> 16) & 0xFF;
				uartBuf.data[5] = (twaiBuf.identifier >> 24) & 0xFF;
				for(int i=0;i<twaiBuf.data_length_code;i++) {
					uartBuf.data[6+i] = twaiBuf.data[i];
				}
				uartBuf.data[14] = 0x55;
				if (xQueueSend(xQueue_uart_tx, &uartBuf, portMAX_DELAY) != pdPASS) {
					ESP_LOGE(CONTROL_TASK_TAG, "xQueueSend Fail");
				}

			}

		}
		vTaskDelay(1);
	}

}

void app_main(void)
{
	// Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

#if CONFIG_UART_INTERFACE
	// Initialize UART
	uart_init();
#else 
	// Initialize USB
	usb_init();
#endif

	// Create Queue
	xQueue_uart_rx = xQueueCreate( 10, sizeof(UART_t) );
	configASSERT( xQueue_uart_rx );
	xQueue_uart_tx = xQueueCreate( 10, sizeof(UART_t) );
	configASSERT( xQueue_uart_tx );
	xQueue_twai_rx = xQueueCreate( 10, sizeof(twai_message_t) );
	configASSERT( xQueue_twai_rx );
	xQueue_twai_tx = xQueueCreate( 10, sizeof(twai_message_t) );
	configASSERT( xQueue_twai_tx );

	// Create Eventgroup
	xEventGroup = xEventGroupCreate();
	configASSERT( xEventGroup );

	// Create Semaphore
	ctrl_task_sem = xSemaphoreCreateBinary();
	configASSERT( ctrl_task_sem );
	xSemaphoreGive(ctrl_task_sem);

	// Create task
#if CONFIG_UART_INTERFACE
	xTaskCreate(uart_rx_task, "uart_rx", 1024*4, NULL, 2, NULL);
	xTaskCreate(uart_tx_task, "uart_tx", 1024*4, NULL, 2, NULL);
#else
	xTaskCreate(tusb_tx_task, "tusb_tx", 1024*4, NULL, 2, NULL);
#endif
	xTaskCreate(twai_receive_task, "twai_rx", 1024*4, NULL, 2, NULL);
	xTaskCreate(twai_transmit_task, "twai_tx", 1024*4, NULL, 2, NULL);
	xTaskCreate(control_task, "control", 1024*4, NULL, 4, NULL);
}

/* UART asynchronous example, that uses separate RX and TX tasks

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
#include "cJSON.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#define RX_BUF_SIZE		128

#if 0
#define TXD_PIN (GPIO_NUM_4)
#define RXD_PIN (GPIO_NUM_5)

#define TX_GPIO_NUM		21
#define RX_GPIO_NUM		22
#endif

#if CONFIG_ENABLE_UDP
/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t s_wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#endif


static const char *UART_TX_TASK_TAG = "UART_TX_TASK";
static const char *UART_RX_TASK_TAG = "UART_RX_TASK";
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

#if CONFIG_ENABLE_UDP
static QueueHandle_t xQueue_wifi_tx;
static const char *TAG = "wifi station";

static int s_retry_num = 0;

static void event_handler(void* arg, esp_event_base_t event_base,
								int32_t event_id, void* event_data)
{
	if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
		esp_wifi_connect();
	} else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
		if (s_retry_num < CONFIG_ESP_MAXIMUM_RETRY) {
			esp_wifi_connect();
			s_retry_num++;
			ESP_LOGI(TAG, "retry to connect to the AP");
		} else {
			xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
		}
		ESP_LOGI(TAG,"connect to the AP fail");
	} else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
		ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
		ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
		s_retry_num = 0;
		xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
	}
}


bool wifi_init_sta(void)
{
	bool ret = false;
	s_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
									ESP_EVENT_ANY_ID,
									&event_handler,
									NULL,
									&instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
									IP_EVENT_STA_GOT_IP,
									&event_handler,
									NULL,
									&instance_got_ip));

	wifi_config_t wifi_config = {
		.sta = {
			.ssid = CONFIG_ESP_WIFI_SSID,
			.password = CONFIG_ESP_WIFI_PASSWORD,
			/* Setting a password implies station will connect to all security modes including WEP/WPA.
			 * However these modes are deprecated and not advisable to be used. Incase your Access point
			 * doesn't support WPA2, these mode can be enabled by commenting below line */
			.threshold.authmode = WIFI_AUTH_WPA2_PSK,

			.pmf_cfg = {
				.capable = true,
				.required = false
			},
		},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA) );
	ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_config) );
	ESP_ERROR_CHECK(esp_wifi_start() );

	ESP_LOGI(TAG, "wifi_init_sta finished.");

	/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
	 * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
		WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
		pdFALSE,
		pdFALSE,
		portMAX_DELAY);

	/* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
	 * happened. */
	if (bits & WIFI_CONNECTED_BIT) {
		ESP_LOGI(TAG, "connected to ap SSID:%s password:%s", CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
		ret = true;
	} else if (bits & WIFI_FAIL_BIT) {
		ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s", CONFIG_ESP_WIFI_SSID, CONFIG_ESP_WIFI_PASSWORD);
	} else {
		ESP_LOGE(TAG, "UNEXPECTED EVENT");
	}

	/* The event will not be processed after unregister */
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
	ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
	vEventGroupDelete(s_wifi_event_group);
	return ret;
}
#endif

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
	uart_driver_install(UART_NUM_1, RX_BUF_SIZE * 2, 0, 0, NULL, 0);
	uart_param_config(UART_NUM_1, &uart_config);
	//uart_set_pin(UART_NUM_1, TXD_PIN, RXD_PIN, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	uart_set_pin(UART_NUM_1, CONFIG_UART_TX_GPIO, CONFIG_UART_RX_GPIO, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
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
				ESP_LOGI(TWAI_RX_TASK_TAG,"twai_receive identifier=0x%"PRIx32" flags=0x%"PRIx32" data_length_code=%d",
				rx_msg.identifier, rx_msg.flags, rx_msg.data_length_code);
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

#if CONFIG_ENABLE_UDP
static const char *WIFI_TASK_TAG = "WIFI_TASK";

static void wifi_broadcast_task(void *arg)
{
	esp_log_level_set(WIFI_TASK_TAG, ESP_LOG_INFO);
	ESP_LOGI(WIFI_TASK_TAG, "Start");

	/* set up address to sendto */
	struct sockaddr_in addr;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(CONFIG_UDP_PORT);
	addr.sin_addr.s_addr = htonl(INADDR_BROADCAST); /* send message to 255.255.255.255 */

	/* create the socket */
	int fd;
	int ret;
	fd = lwip_socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP ); // Create a UDP socket.
	LWIP_ASSERT("fd >= 0", fd >= 0);

	twai_message_t twaiBuf;

	while(1) {
		xQueueReceive(xQueue_wifi_tx, &twaiBuf, portMAX_DELAY);
		ESP_LOGI(WIFI_TASK_TAG,"twai_receive identifier=0x%"PRIx32" flags=0x%"PRIx32"-0x%x-0x%x data_length_code=%d",
			twaiBuf.identifier, twaiBuf.flags, twaiBuf.extd, twaiBuf.rtr, twaiBuf.data_length_code);

		// JSON Serialize
		cJSON *root = cJSON_CreateObject();
		if (twaiBuf.rtr == 0) {
			cJSON_AddStringToObject(root, "Type", "Data frame");
		} else {
			cJSON_AddStringToObject(root, "Type", "Remote frame");
		}
		if (twaiBuf.extd == 0) {
			cJSON_AddStringToObject(root, "Format", "Standard frame");
		} else {
			cJSON_AddStringToObject(root, "Format", "Extended frame");
		}
		cJSON_AddNumberToObject(root, "ID", twaiBuf.identifier);
		cJSON_AddNumberToObject(root, "Length", twaiBuf.data_length_code);
		cJSON *intArray;
		int i_numbers[8];
		for(int i=0;i<twaiBuf.data_length_code;i++) {
			i_numbers[i] = twaiBuf.data[i];
		}
		
		if (twaiBuf.data_length_code > 0) {
			intArray = cJSON_CreateIntArray(i_numbers, twaiBuf.data_length_code);
			cJSON_AddItemToObject(root, "Data", intArray);
		}
		const char *my_json_string = cJSON_Print(root);
		ESP_LOGI(WIFI_TASK_TAG, "my_json_string\n%s",my_json_string);

		// Send UDP broadcast
		int ret = lwip_sendto(fd, my_json_string, strlen(my_json_string), 0, (struct sockaddr *)&addr, sizeof(addr));
		LWIP_ASSERT("ret == strlen(my_json_string)", ret == strlen(my_json_string));

		// Cleanup
		free((void *)my_json_string);
		cJSON_Delete(root);

	}

	/* close socket. Don't reach here.*/
	ret = lwip_close(fd);
	LWIP_ASSERT("ret == 0", ret == 0);
	vTaskDelete( NULL );

}
#endif


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

#if CONFIG_ENABLE_UDP
			if (xQueueSend(xQueue_wifi_tx, &twaiBuf, portMAX_DELAY) != pdPASS) {
				ESP_LOGE(CONTROL_TASK_TAG, "xQueueSend Fail");
			}
#endif

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

#if CONFIG_ENABLE_UDP
	// WiFi initialize
	if (wifi_init_sta() == false) {
		while(1) vTaskDelay(10);
	}
#endif

	// uart initialize
	uart_init();

	// Create Queue
	xQueue_uart_rx = xQueueCreate( 10, sizeof(UART_t) );
	configASSERT( xQueue_uart_rx );
	xQueue_uart_tx = xQueueCreate( 10, sizeof(UART_t) );
	configASSERT( xQueue_uart_tx );
	xQueue_twai_rx = xQueueCreate( 10, sizeof(twai_message_t) );
	configASSERT( xQueue_twai_rx );
	xQueue_twai_tx = xQueueCreate( 10, sizeof(twai_message_t) );
	configASSERT( xQueue_twai_tx );
#if CONFIG_ENABLE_UDP
	xQueue_wifi_tx = xQueueCreate( 10, sizeof(twai_message_t) );
	configASSERT( xQueue_wifi_tx );
#endif

	// Create Eventgroup
	xEventGroup = xEventGroupCreate();
	configASSERT( xEventGroup );

	// Create Semaphore
	ctrl_task_sem = xSemaphoreCreateBinary();
	configASSERT( ctrl_task_sem );
	xSemaphoreGive(ctrl_task_sem);

	// Create task
	xTaskCreate(uart_rx_task, "uart_rx", 1024*4, NULL, 2, NULL);
	xTaskCreate(uart_tx_task, "uart_tx", 1024*4, NULL, 2, NULL);
	xTaskCreate(twai_receive_task, "twai_rx", 1024*4, NULL, 2, NULL);
	xTaskCreate(twai_transmit_task, "twai_tx", 1024*4, NULL, 2, NULL);
#if CONFIG_ENABLE_UDP
	xTaskCreate(wifi_broadcast_task, "wifi_tx", 1024*4, NULL, 2, NULL);
#endif
	xTaskCreate(control_task, "control", 1024*4, NULL, 4, NULL);
}

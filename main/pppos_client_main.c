/* PPPoS Client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "dht.h"
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "mqtt_client.h"
#include "esp_modem.h"
#include "esp_modem_netif.h"
#include "esp_log.h"
#include "sim800.h"
#include "bg96.h"
#include "nvs_flash.h"



/* prender el sim sin que muera*/
#include "driver/gpio.h"

#define SIM800l_PWR_KEY (4)
#define SIM800l_PWR (23)
#define SIM800l_RST (5)

// Cosas de la temperatura

#define Pila 1024
#define tamCola 1
#define tamMSN 53

//Para el gps

#define BUF_SIZE (1024)
#include "NMEA_setting.h"

//Para saber si se leyo la temperatura y el gps

extern uint8_t error_temp
extern uint8_t error_gps



//#define BROKER_URL "mqtt://kike:Kike3355453@mqtt.tiosplatform.com"
#define BROKER_URL "201.211.92.114:5500"

static const char *TAG = "pppos_example";
EventGroupHandle_t event_group = NULL;
const int CONNECT_BIT = BIT0;
const int STOP_BIT = BIT1;
const int GOT_DATA_BIT = BIT2;

//Para las 3 tareas principales
  const int BEGIN_TASK1 = BIT3;

  const int BEGIN_TASK2 = BIT4;

  const int BEGIN_TASK3 = BIT5;

#if CONFIG_EXAMPLE_SEND_MSG
/**
 * @brief This example will also show how to send short message using the infrastructure provided by esp modem library.
 * @note Not all modem support SMG.
 *
 */
static esp_err_t example_default_handle(modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_FAIL;
    if (strstr(line, MODEM_RESULT_CODE_SUCCESS)) {
        err = esp_modem_process_command_done(dce, MODEM_STATE_SUCCESS);
    } else if (strstr(line, MODEM_RESULT_CODE_ERROR)) {
        err = esp_modem_process_command_done(dce, MODEM_STATE_FAIL);
    }
    return err;
}

static esp_err_t example_handle_cmgs(modem_dce_t *dce, const char *line)
{
    esp_err_t err = ESP_FAIL;
    if (strstr(line, MODEM_RESULT_CODE_SUCCESS)) {
        err = esp_modem_process_command_done(dce, MODEM_STATE_SUCCESS);
    } else if (strstr(line, MODEM_RESULT_CODE_ERROR)) {
        err = esp_modem_process_command_done(dce, MODEM_STATE_FAIL);
    } else if (!strncmp(line, "+CMGS", strlen("+CMGS"))) {
        err = ESP_OK;
    }
    return err;
}

#define MODEM_SMS_MAX_LENGTH (128)
#define MODEM_COMMAND_TIMEOUT_SMS_MS (120000)
#define MODEM_PROMPT_TIMEOUT_MS (10)

static esp_err_t example_send_message_text(modem_dce_t *dce, const char *phone_num, const char *text)
{
    modem_dte_t *dte = dce->dte;
    dce->handle_line = example_default_handle;
    /* Set text mode */
    if (dte->send_cmd(dte, "AT+CMGF=1\r", MODEM_COMMAND_TIMEOUT_DEFAULT) != ESP_OK) {
        ESP_LOGE(TAG, "send command failed");
        goto err;
    }
    if (dce->state != MODEM_STATE_SUCCESS) {
        ESP_LOGE(TAG, "set message format failed");
        goto err;
    }
    ESP_LOGD(TAG, "set message format ok");
    /* Specify character set */
    dce->handle_line = example_default_handle;
    if (dte->send_cmd(dte, "AT+CSCS=\"GSM\"\r", MODEM_COMMAND_TIMEOUT_DEFAULT) != ESP_OK) {
        ESP_LOGE(TAG, "send command failed");
        goto err;
    }
    if (dce->state != MODEM_STATE_SUCCESS) {
        ESP_LOGE(TAG, "set character set failed");
        goto err;
    }
    ESP_LOGD(TAG, "set character set ok");
    /* send message */
    char command[MODEM_SMS_MAX_LENGTH] = {0};
    int length = snprintf(command, MODEM_SMS_MAX_LENGTH, "AT+CMGS=\"%s\"\r", phone_num);
    /* set phone number and wait for "> " */
    dte->send_wait(dte, command, length, "\r\n> ", MODEM_PROMPT_TIMEOUT_MS);
    /* end with CTRL+Z */
    snprintf(command, MODEM_SMS_MAX_LENGTH, "%s\x1A", text);
    dce->handle_line = example_handle_cmgs;
    if (dte->send_cmd(dte, command, MODEM_COMMAND_TIMEOUT_SMS_MS) != ESP_OK) {
        ESP_LOGE(TAG, "send command failed");
        goto err;
    }
    if (dce->state != MODEM_STATE_SUCCESS) {
        ESP_LOGE(TAG, "send message failed");
        goto err;
    }
    ESP_LOGD(TAG, "send message ok");
    return ESP_OK;
err:
    return ESP_FAIL;
}
#endif

static void modem_event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    switch (event_id) {
    case ESP_MODEM_EVENT_PPP_START:
        ESP_LOGI(TAG, "Modem PPP Started");
        break;
    case ESP_MODEM_EVENT_PPP_STOP:
        ESP_LOGI(TAG, "Modem PPP Stopped");
        xEventGroupSetBits(event_group, STOP_BIT);
        break;
    case ESP_MODEM_EVENT_UNKNOWN:
        ESP_LOGW(TAG, "Unknow line received: %s", (char *)event_data);
        break;
    default:
        break;
    }
}

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
        msg_id = esp_mqtt_client_subscribe(client, "/topic/esp-pppos", 0);
        ESP_LOGI(TAG, "sent subscribe successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
        break;
    case MQTT_EVENT_SUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
        msg_id = esp_mqtt_client_publish(client, "/topic/esp-pppos", "esp32-pppos", 0, 0, 0);
        ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
        break;
    case MQTT_EVENT_UNSUBSCRIBED:
        ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_PUBLISHED:
        ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
        break;
    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT_EVENT_DATA");
        printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
        printf("DATA=%.*s\r\n", event->data_len, event->data);
        xEventGroupSetBits(event_group, GOT_DATA_BIT);
        break;
    case MQTT_EVENT_ERROR:
        ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
        break;
    default:
        ESP_LOGI(TAG, "MQTT other event id: %d", event->event_id);
        break;
    }
    return ESP_OK;
}

static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "PPP state changed event %d", event_id);
    if (event_id == NETIF_PPP_ERRORUSER) {
        /* User interrupted event from esp-netif */
        esp_netif_t *netif = event_data;
        ESP_LOGI(TAG, "User interrupted event from netif:%p", netif);
    }
}


static void on_ip_event(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "IP event! %d", event_id);
    if (event_id == IP_EVENT_PPP_GOT_IP) {
        esp_netif_dns_info_t dns_info;

        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        esp_netif_t *netif = event->esp_netif;

        ESP_LOGI(TAG, "Modem Connect to PPP Server");
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
        ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
        ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.ip));
        esp_netif_get_dns_info(netif, 0, &dns_info);
        ESP_LOGI(TAG, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        esp_netif_get_dns_info(netif, 1, &dns_info);
        ESP_LOGI(TAG, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
        ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
        xEventGroupSetBits(event_group, CONNECT_BIT);

        ESP_LOGI(TAG, "GOT ip event!!!");
    } else if (event_id == IP_EVENT_PPP_LOST_IP) {
        ESP_LOGI(TAG, "Modem Disconnect from PPP Server");
    }
}

void Mandar_mensaje(void *P)
{
	char message[318] = "Welcome to ESP32!";
	uint8_t vuelta_men = 0;

	printf("Entre en mandar mensaje \r\n");
		for(;;){

		xEventGroupWaitBits(event_group,BEGIN_TASK2,true,true,portMAX_DELAY);

#if CONFIG_LWIP_PPP_PAP_SUPPORT
    esp_netif_auth_type_t auth_type = NETIF_PPP_AUTHTYPE_PAP;
#elif CONFIG_LWIP_PPP_CHAP_SUPPORT
    esp_netif_auth_type_t auth_type = NETIF_PPP_AUTHTYPE_CHAP;
#else
#error "Unsupported AUTH Negotiation"
#endif

    ESP_ERROR_CHECK(esp_netif_init());
    if (vuelta_men == 0){
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID, &on_ip_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL));
    }



    	ESP_LOGI(TAG, "La latitud prom es: %f", gps_data.latitude_prom);
    	ESP_LOGI(TAG, "La latitud dir es: %s", gps_data.latitude_direct);
        ESP_LOGI(TAG, "La longitud prom es: %f", gps_data.longitude_prom);
      	ESP_LOGI(TAG, "La longitud dir es: %s", gps_data.longitude_direct);




        /* Configurar los niveles de las salidas y el pulso necesario para prender el SIM800l*/
        gpio_set_level(SIM800l_PWR, 1);
        gpio_set_level(SIM800l_RST, 1);
        gpio_set_level(SIM800l_PWR_KEY, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        gpio_set_level(SIM800l_PWR_KEY, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        gpio_set_level(SIM800l_PWR_KEY, 1);


        vTaskDelay(5000 / portTICK_PERIOD_MS);

    // Init netif object
    if (vuelta_men == 0){
    esp_netif_config_t cfg = ESP_NETIF_DEFAULT_PPP();
    esp_netif_t *esp_netif = esp_netif_new(&cfg);
    assert(esp_netif);
    }

    /* create dte object */
    esp_modem_dte_config_t config = ESP_MODEM_DTE_DEFAULT_CONFIG();
    modem_dte_t *dte = esp_modem_dte_init(&config);
    vTaskDelay(5000 / portTICK_PERIOD_MS);
    /* Register event handler */
    ESP_ERROR_CHECK(esp_modem_set_event_handler(dte, modem_event_handler, ESP_EVENT_ANY_ID, NULL));
    vTaskDelay(3000 / portTICK_PERIOD_MS);
    /* create dce object */
#if CONFIG_EXAMPLE_MODEM_DEVICE_SIM800
    modem_dce_t *dce = sim800_init(dte);
#elif CONFIG_EXAMPLE_MODEM_DEVICE_BG96
    modem_dce_t *dce = bg96_init(dte);
#else
#error "Unsupported DCE"
#endif
    ESP_ERROR_CHECK(dce->set_flow_ctrl(dce, MODEM_FLOW_CONTROL_NONE));
    ESP_ERROR_CHECK(dce->store_profile(dce));
    /* Print Module ID, Operator, IMEI, IMSI */
    ESP_LOGI(TAG, "Module: %s", dce->name);
    ESP_LOGI(TAG, "Operator: %s", dce->oper);
    ESP_LOGI(TAG, "IMEI: %s", dce->imei);
    ESP_LOGI(TAG, "IMSI: %s", dce->imsi);
    /* Get signal quality */
    uint32_t rssi = 0, ber = 0;
    ESP_ERROR_CHECK(dce->get_signal_quality(dce, &rssi, &ber));
    ESP_LOGI(TAG, "rssi: %d, ber: %d", rssi, ber);
    /* Get battery voltage */
    uint32_t voltage = 0, bcs = 0, bcl = 0;
    ESP_ERROR_CHECK(dce->get_battery_status(dce, &bcs, &bcl, &voltage));
    ESP_LOGI(TAG, "Battery voltage: %d mV", voltage);

    /* Para mandar mensajes con menuconfig se puede configurar el numero que recibira el mensaje
     y el mensaje va a ser el la variable message, recordando que tiene un limite de caracteres
     * */

    //Se verifica si se logro medir la temperatura y se manda el mensaje correspondiente
    if (error_temp == 0){
		#if CONFIG_EXAMPLE_SEND_MSG
    	sprintf(message,"La humedad es: %c%c.%c  %% y la temperatura es: %c%c.%c C",form1.Humedad1[0],form1.Humedad1[1],form1.Humedad1[2],form1.Temperatura1[0],form1.Temperatura1[1],form1.Temperatura1[2]);
    	ESP_ERROR_CHECK(example_send_message_text(dce, CONFIG_EXAMPLE_SEND_MSG_PEER_PHONE_NUMBER, message));
    	ESP_LOGI(TAG, "Send send message [%s] ok", message);
		#endif
    } else {
    	error_temp = 0;
		#if CONFIG_EXAMPLE_SEND_MSG
    	sprintf(message,"No se logro medir la temepratura. Revisar las conexiones.");
    	ESP_ERROR_CHECK(example_send_message_text(dce, CONFIG_EXAMPLE_SEND_MSG_PEER_PHONE_NUMBER, message));
    	ESP_LOGI(TAG, "Send send message [%s] ok", message);
		#endif
    }

    if (error_gps == 0){

    	if (gps_data.year == 20){
			#if CONFIG_EXAMPLE_SEND_MSG
    		sprintf(message,"La latitud es: %.4f %s y la longitud es: %.4f %s",gps_data.latitude_prom,gps_data.latitude_direct,gps_data.longitude_prom,gps_data.longitude_direct);
    		ESP_ERROR_CHECK(example_send_message_text(dce, CONFIG_EXAMPLE_SEND_MSG_PEER_PHONE_NUMBER, message));
    		ESP_LOGI(TAG, "Send send message [%s] ok", message);
			#endif

			#if CONFIG_EXAMPLE_SEND_MSG
    		sprintf(message,"Las medidas se realizaron el %d de %s de 20%d a las %d horas con %d minutos y %d segundos",gps_data.day,gps_data.mes,gps_data.year,gps_data.hour,gps_data.minute,gps_data.second);
    		ESP_ERROR_CHECK(example_send_message_text(dce, CONFIG_EXAMPLE_SEND_MSG_PEER_PHONE_NUMBER, message));
    		ESP_LOGI(TAG, "Send send message [%s] ok", message);
			#endif
    	} else{
			#if CONFIG_EXAMPLE_SEND_MSG
    		sprintf(message,"No se logro conectar a la red GPS.");
    		ESP_ERROR_CHECK(example_send_message_text(dce, CONFIG_EXAMPLE_SEND_MSG_PEER_PHONE_NUMBER, message));
    		ESP_LOGI(TAG, "Send send message [%s] ok", message);
			#endif
    	}
    } else {
		#if CONFIG_EXAMPLE_SEND_MSG
    	sprintf(message,"No se logro conectar con el modulo GPS.");
    	ESP_ERROR_CHECK(example_send_message_text(dce, CONFIG_EXAMPLE_SEND_MSG_PEER_PHONE_NUMBER, message));
    	ESP_LOGI(TAG, "Send send message [%s] ok", message);
	#endif

    }




    /* Power down module */
    ESP_ERROR_CHECK(dce->power_down(dce));
    ESP_LOGI(TAG, "Power down");
    ESP_ERROR_CHECK(dce->deinit(dce));
    ESP_ERROR_CHECK(dte->deinit(dte));
    ESP_LOGI(TAG, "Power down2");
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    xEventGroupClearBits(event_group, BEGIN_TASK2);
    xEventGroupSetBits(event_group, BEGIN_TASK1);
    vuelta_men = 1;

	}
}

//xQueueHandle Cola_sensor;

void app_main(void)
{
	nvs_flash_init();
//	Cola_sensor = xQueueCreate(tamCola,tamMSN);

	 /*Configurar inicio del SIM800l*/
	        /*Poner los pines como GPIO*/
	        gpio_pad_select_gpio(SIM800l_PWR_KEY);
	        gpio_pad_select_gpio(SIM800l_PWR);
	        gpio_pad_select_gpio(SIM800l_RST);
	        /*Configurar los GPIO como output, el RST como Open Drain por seguridad*/
	        gpio_set_direction(SIM800l_PWR_KEY, GPIO_MODE_OUTPUT);
	        gpio_set_direction(SIM800l_PWR, GPIO_MODE_OUTPUT);
	        gpio_set_direction(SIM800l_RST, GPIO_MODE_OUTPUT_OD);

	event_group = xEventGroupCreate();

	xTaskCreatePinnedToCore(&TareaDHT, "TareaDHT", Pila*3, NULL, 8, NULL,0);
	xTaskCreatePinnedToCore(&echo_task, "uart_echo_task", 1024*6, NULL, 6, NULL,0);
	xTaskCreatePinnedToCore(&Mandar_mensaje, "Mandar mensaje", 1024*3, NULL, 4, NULL,0);


	 xEventGroupSetBits(event_group, BEGIN_TASK1);


}

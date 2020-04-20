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
#include "driver/gpio.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "mqtt_client.h"
#include "esp_modem.h"
#include "esp_modem_netif.h"
#include "esp_log.h"
#include "sim800.h"
#include "bg96.h"
#include "nvs_flash.h"
#include "freertos/queue.h"
#include "tracking.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"



/* prender el sim sin que muera*/
#include "driver/gpio.h"

#define SIM800l_PWR_KEY (4)
#define SIM800l_PWR (23)
#define SIM800l_RST (5)

// Cosas de la temperatura

//#define Pila 1024


//Para el gps

#define BUF_SIZE (1024)
#include "NMEA_setting.h"


//Para las colas

QueueHandle_t xQueue_temp;
QueueHandle_t xQueue_gps;

//Para el limite de la temperatura
uint8_t limite_a =0;
uint8_t limite_b =0;
uint8_t limite_c = 0;




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

  const int SYNC_BIT_TASK1 = BIT6;

  const int SYNC_BIT_TASK2 = BIT7;

//Para conocer el estado de la puerta
  //puerta a es el estado actual de la puerta, puerta b es para saber si se abrio y cambia si se cerro
  //despues de mandar el mensaje, puerta c es para saltar a mandar el mensaje inmediatamente pero solo
  //cuando se abre la puerta, si se mantiene abierta esperara el ciclo normal del programa
uint8_t puerta_a = 0;
//uint8_t puerta_b = 0;
uint8_t puerta_c = 0;
e_Puerta puerta_b = 0;






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




void Mandar_mensaje2(void *P)
{
	char message[318] = "Welcome to ESP32!";
	uint8_t  led_gsm = 0;
	gps_data_t gps_data2;
	AM2301_data_t Thum2;
	message_data_t message_data;


	printf("Entre en mandar mensaje \r\n");
		for(;;){

		xEventGroupSync(event_group,BEGIN_TASK1,0xC0,portMAX_DELAY);
	    xEventGroupClearBits(event_group, SYNC_BIT_TASK1);
	   	xEventGroupClearBits(event_group, SYNC_BIT_TASK2);

		//El led prendera 1 segundo al entrar en esta etapa
    	if (led_gsm == 0){
            gpio_set_level(GPIO_NUM_27, 1);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            gpio_set_level(GPIO_NUM_27, 0);
            led_gsm = 1;
    	}

    /* Para mandar mensajes con menuconfig se puede configurar el numero que recibira el mensaje
     y el mensaje va a ser el la variable message, recordando que tiene un limite de caracteres
      */
    xQueueReceive(xQueue_temp,&Thum2,portMAX_DELAY);
	sprintf(message_data.Humedad, "%f",Thum2.Prom_hum[Thum2.pos_temp-1]);
	sprintf(message_data.Temperatura, "%f",Thum2.Prom_temp[Thum2.pos_temp-1]);
    //Se verifica si se logro medir la temperatura y se manda el mensaje correspondiente
	 if (Thum2.error_temp == 0){
		 switch(limite_b){
		 case 0:
		    	sprintf(message,"La humedad es: %.1f  %% y la temperatura es: %.1f C",Thum2.Prom_hum[Thum2.pos_temp-1],Thum2.Prom_temp[Thum2.pos_temp-1]);
		    	ESP_LOGI(TAG, "[%s]", message);
		 break;
		 case 1:
		    sprintf(message,"La temperatura se salio de los limites. La humedad es: %.1f  %% y la temperatura es: %.1f C",Thum2.Prom_hum[Thum2.pos_temp-1],Thum2.Prom_temp[Thum2.pos_temp-1]);
		    ESP_LOGI(TAG, "[%s]", message);
		    if (limite_a == 0){
		    	limite_b = 0;
		    }
	     break;
		 }
	 } else {
    	Thum.error_temp = 0;
    	sprintf(message,"No se logro medir la temperatura. Revisar las conexiones.");
    	ESP_LOGI(TAG, "[%s] ", message);
    }

    xQueueReceive(xQueue_gps,&gps_data2,portMAX_DELAY);
    sprintf(message_data.Latitude, "%f",gps_data2.latitude_prom);
    sprintf(message_data.Longitude, "%f",gps_data2.latitude_prom);


    switch (puerta_b){
    case P_cerrada:
    // Verifico si no hubo error al conectarse a al modulo GPS
        if (gps_data2.error_gps == 0){
        	//Verifico si se conecto a la red GPS viendo si devolvio que es el a;o 2020
        	if (gps_data2.year == 20){

        		sprintf(message,"La latitud es: %.4f %s y la longitud es: %.4f %s",gps_data2.latitude_prom,gps_data2.latitude_direct,gps_data2.longitude_prom,gps_data2.longitude_direct);
        		ESP_LOGI(TAG, "[%s] ", message);
        		sprintf(message,"Las medidas se realizaron el %d de %s de 20%d a las %d horas con %d minutos y %d segundos",gps_data2.day,gps_data2.mes,gps_data2.year,gps_data2.hour,gps_data2.minute,gps_data2.second);
            	ESP_LOGI(TAG, "[%s] ", message);
        	} else{
        		sprintf(message,"No se logro conectar a la red GPS.");
        		ESP_LOGI(TAG, "[%s] ", message);
        	}
        } else{
        	gps_data.error_gps = 0;
    		sprintf(message,"No se logro conectar con el modulo GPS.");
    		ESP_LOGI(TAG, "[%s] ", message);
        }
    break;
    case P_abierta:
    	// Verifico si no hubo error al conectarse a al modulo GPS
    	if (gps_data2.error_gps == 0){
    	//Verifico si se conecto a la red GPS viendo si devolvio que es el a;o 2020
    		if (gps_data2.year == 20){
    			sprintf(message,"La puerta fue abierta. La latitud es: %.4f %s y la longitud es: %.4f %s",gps_data2.latitude_prom,gps_data2.latitude_direct,gps_data2.longitude_prom,gps_data2.longitude_direct);
			ESP_LOGI(TAG, "[%s] ", message);
    			if (puerta_a == 0){
    				puerta_b = 0;
    				puerta_c = 0;
    			}
    	     sprintf(message,"Las medidas se realizaron el %d de %s de 20%d a las %d horas con %d minutos y %d segundos",gps_data2.day,gps_data2.mes,gps_data2.year,gps_data2.hour,gps_data2.minute,gps_data2.second);
    	     ESP_LOGI(TAG, "[%s] ", message);
    	    } else{
    	    	sprintf(message,"La puerta se abrio, pero no se logro conectar a la red GPS.");
    	    	ESP_LOGI(TAG, "[%s] ", message);
    			if (puerta_a == 0){
    				puerta_b = 0;
    				puerta_c = 0;
    			}
    	    }
    	} else{
			gps_data.error_gps = 0;
			sprintf(message,"La puerta se abrio, pero no se logro conectar con el modulo GPS.");
			ESP_LOGI(TAG, "[%s] ", message);
			if (puerta_a == 0){
				puerta_b = 0;
				puerta_c = 0;
			}
    	}
    break;
    }
    led_gsm = 0;
	}
}

void app_main(void)
{
	int a = 0;
	esp_err_t err = nvs_flash_init();
	    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
	        // NVS partition was truncated and needs to be erased
	        // Retry nvs_flash_init
	        ESP_ERROR_CHECK(nvs_flash_erase());
	        err = nvs_flash_init();
	    }
	    ESP_ERROR_CHECK( err );

	    // Open
	    printf("\n");
	    printf("Opening Non-Volatile Storage (NVS) handle... ");
	    nvs_handle_t my_handle;
	    err = nvs_open("storage2", NVS_READWRITE, &my_handle);
	    if (err != ESP_OK) {
	        printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
	    } else {
	        printf("Done\n");

	        // Read
	        printf("Reading restart counter from NVS ... ");
	        int32_t restart_counter = 0; // value will default to 0, if not set yet in NVS
	        err = nvs_get_i32(my_handle, "restart_counter", &restart_counter);
	        switch (err) {
	            case ESP_OK:
	                printf("Done\n");
	                printf("Restart counter = %d\n", restart_counter);
	                break;
	            case ESP_ERR_NVS_NOT_FOUND:
	                printf("The value is not initialized yet!\n");
	                break;
	            default :
	                printf("Error (%s) reading!\n", esp_err_to_name(err));
	        }

	        // Write
	        printf("Updating restart counter in NVS ... ");
	        restart_counter++;
	        a = restart_counter;
	        err = nvs_set_i32(my_handle, "restart_counter", restart_counter);
	        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

	        // Commit written value.
	        // After setting any values, nvs_commit() must be called to ensure changes are written
	        // to flash storage. Implementations may write to storage at other times,
	        // but this is not guaranteed.
	        printf("Committing updates in NVS ... ");
	        err = nvs_commit(my_handle);
	        printf((err != ESP_OK) ? "Failed!\n" : "Done\n");

	        // Close
	        nvs_close(my_handle);
	    }

	/*Configurar inicio del SIM800l*/
	/*Poner los pines como GPIO*/
	gpio_pad_select_gpio(SIM800l_PWR_KEY);
	gpio_pad_select_gpio(SIM800l_PWR);
	gpio_pad_select_gpio(SIM800l_RST);
	/*Configurar los GPIO como output, el RST como Open Drain por seguridad*/
	gpio_set_direction(SIM800l_PWR_KEY, GPIO_MODE_OUTPUT);
	gpio_set_direction(SIM800l_PWR, GPIO_MODE_OUTPUT);
	gpio_set_direction(SIM800l_RST, GPIO_MODE_OUTPUT_OD);

	//Configurar los pines para el sensor de presencia de la puerta
	gpio_pad_select_gpio(GPIO_NUM_14);
	gpio_set_direction(GPIO_NUM_14, GPIO_MODE_INPUT);

	//Para usar un led indicador de etapa
	gpio_pad_select_gpio(GPIO_NUM_27);
	gpio_set_direction(GPIO_NUM_27, GPIO_MODE_OUTPUT);
	gpio_set_level(GPIO_NUM_27, 0);

	//Creo el grupo de eventos y las colas
	event_group = xEventGroupCreate();
	xQueue_temp = xQueueCreate(1, sizeof(AM2301_data_t));
	xQueue_gps = xQueueCreate(1, sizeof(gps_data_t));


	xTaskCreatePinnedToCore(&TareaDHT, "TareaDHT", 1024*3, NULL, 5, NULL,0);
	xTaskCreatePinnedToCore(&echo_task, "uart_echo_task", 1024*8, NULL, 5, NULL,1);
	xTaskCreatePinnedToCore(&Mandar_mensaje2, "Mandar mensaje2", 1024*6, NULL, 6, NULL,0);
//	La tarea dht se inicia en sync desde mandar mensaje
	xEventGroupSetBits(event_group, BEGIN_TASK2);


	//Espera a que la puerta se abra para dar la senal
	while(1){
		//Si el nivel de la puerta es 1 entonces uso dos variables para controlar este nivel
		//puerta_a sirve para saber si la puerta esta abierta o cerrada en todo momento
		//puerta_b se usa para tener una referencia de cuando la puerta se abrio y no repetir
		//el uso de las tareas
		if (gpio_get_level(GPIO_NUM_14) == 0){
			vTaskDelay(200 / portTICK_PERIOD_MS);
			puerta_a = 1;
			if (puerta_b == 0){
				puerta_b = 1;
			}
		}
		if (gpio_get_level(GPIO_NUM_14) == 1){
			vTaskDelay(200 / portTICK_PERIOD_MS);
			puerta_a = 0;
		}
		//Ahora que ya se si la puerta se abrio o no, falta activar la tarea
		//puerta_c sirve para activar inmediatamente la funcion de mandar mensaje pero solo una vez
		//luego debe de seguir el ciclo normal del programa cambiando unicamente
		//lo que dicen los mensajes.

		if (puerta_b == 1 && puerta_c == 0 ){
			puerta_c = 1;
			xEventGroupSetBits(event_group, SYNC_BIT_TASK1);
			xEventGroupSetBits(event_group, SYNC_BIT_TASK2);
		}
		printf("Restart counter = %d\n", a);
		vTaskDelay(1000 / portTICK_PERIOD_MS);

	}



}

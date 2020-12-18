/*Tracking_main
 * Author: Alejandro Antunes
 * e-mail: aleantunes95@gmail.com
 * Date: 09-10-2020
 * MIT License
 * As it is described in the readme file
 *
*/

#include <AM2301.h>
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "tracking.h"
#include "NMEA_setting.h"
//#include "esp_sleep.h"
//#include "driver/rtc_io.h"

/* Configuracion de pines del SIM800L*/

#define SIM800l_PWR_KEY (4)
#define SIM800l_PWR (23)
#define SIM800l_RST (5)

/* Pin de sensor de la puerta*/
#define PIN_PUERTA (GPIO_NUM_14)

//Para las colas

QueueHandle_t xQueue_temp;
QueueHandle_t xQueue_gps;

//Para el limite de la temperatura
uint8_t limite_a = 0;
uint8_t limite_b = 0;


EventGroupHandle_t event_group = NULL;

/*const int CONNECT_BIT = BIT0;
const int STOP_BIT = BIT1;
const int GOT_DATA_BIT = BIT2;*/

//Para las 3 tareas principales
const int BEGIN_TASK1 = BIT3;
const int BEGIN_TASK2 = BIT4;
const int BEGIN_TASK3 = BIT5;
const int SYNC_BIT_TASK1 = BIT6;
const int SYNC_BIT_TASK2 = BIT7;

/*Para conocer el estado de la puerta puerta a es el estado actual de la puerta, puerta b es para
 *  saber si se abrio y cambia si se cerro despues de mandar el mensaje, puerta c es para saltar
 *  a mandar el mensaje inmediatamente pero solo cuando se abre la puerta, si se mantiene
 *  abierta esperara el ciclo normal del programa*/
uint8_t puerta_a = 0, puerta_c = 0, puerta_d = 0, puerta_e = 0;
e_Puerta puerta_b = 0;

/* Para el envio de datos que recibe el uart 1*/
 QueueHandle_t uart1_queue;
 QueueHandle_t Cola1;
 QueueHandle_t Datos_uart1;



void app_main(void){
	//Esta parte inicial es para verificar las veces que se reinicia el esp32
	/*int a = 0;
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
	    }*/
	//Quitar el comentario de abajo cuando se borre la parte donde se guarda en la memoria flash
	nvs_flash_init();
	Configurar_UARTs();

	/* Colas para las tareas de echo entre uart 0 y  uart 1*/
	Cola1= xQueueCreate(1, sizeof(struct TRAMA));
	Datos_uart1 = xQueueCreate(1, sizeof(struct TRAMA));

	/*Tarea de echo entre uart 1 a uart 0*/
	xTaskCreate(echo_U1toU0, "Echo a uart0", 4*1024, NULL, 2, NULL);
	xTaskCreate(uart1_event_task, "uart1_event_task", 10*2048, NULL, 1, NULL);

	/*Configurar pines del SIM800l*/
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

	//Configurar el led para saber si prendio el ESP32 (tarjeta TTGO Tcall SIM800L)
	gpio_pad_select_gpio(GPIO_NUM_13);
	gpio_set_direction(GPIO_NUM_13, GPIO_MODE_OUTPUT);
	gpio_set_level(GPIO_NUM_13, 1);

	/*Crear el grupo de eventos y las colas para sensor de temperatura,gps y gsm*/
	event_group = xEventGroupCreate();
	xQueue_temp = xQueueCreate(1, sizeof(AM2301_data_t));
	xQueue_gps = xQueueCreate(1, sizeof(gps_data_t));

	xTaskCreatePinnedToCore(&TareaAM2301, "TareaAM2301", 1024*4, NULL, 5, NULL,0);
	xTaskCreatePinnedToCore(&GNSS_task, "GNSS_task", 1024*8, NULL, 5, NULL,1);
	xTaskCreatePinnedToCore(&Mandar_mensaje, "Mandar mensaje", 1024*10, NULL, 6, NULL,1);
//	La tarea dht se inicia en sync desde mandar mensaje
	xEventGroupSetBits(event_group, BEGIN_TASK2);


	//Ciclo que que verifica el estado de la puerta
	while(1){
		/*Si el nivel de la puerta es 1 entonces uso dos variables para controlar este nivel
		* puerta_a sirve para saber si la puerta esta abierta o cerrada en todo momento
		* puerta_b se usa para tener una referencia de cuando la puerta se abrio y no repetir
		* el uso de las tareas*/
		if (gpio_get_level(PIN_PUERTA) == 0){
			vTaskDelay(200 / portTICK_PERIOD_MS);
			puerta_a = 1;
			if (puerta_b == 0){
					puerta_b = 1;
			}
		}
		if (gpio_get_level(PIN_PUERTA) == 1){
			vTaskDelay(200 / portTICK_PERIOD_MS);
			puerta_a = 0;
		}
		/*Ahora que ya se si la puerta se abrio o no, falta activar la tarea
		*puerta_c sirve para activar inmediatamente la funcion de mandar mensaje pero solo una vez
		*luego debe de seguir el ciclo normal del programa cambiando unicamente
		*lo que dicen los mensajes.*/
		if (puerta_b == 1 && puerta_c == 0 ){
			puerta_c = 1;
			xEventGroupSetBits(event_group, SYNC_BIT_TASK1);
			xEventGroupSetBits(event_group, SYNC_BIT_TASK2);
		}
	//	printf("Restart counter = %d\n", a);
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

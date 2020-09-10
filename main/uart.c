/*
 * uart.c
 *
 *  Created on: Jul 22, 2020
 *      Author: jose
 */


#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "tracking.h"


/*Definir los pines del uart conectado al modulo SIM800l */
#define TX1 														GPIO_NUM_27                                                                //
#define RX1 														GPIO_NUM_26
#define LED 														GPIO_NUM_13
#define EX_UART_NUM 												UART_NUM_0
#define PATTERN_CHR_NUM    											3
#define RD_BUF_SIZE 												BUF_SIZE

/*Definir los pines del uart conectado al modulo GPS */
#define GPS_TXD  (GPIO_NUM_18)
#define GPS_RXD  (GPIO_NUM_0)
#define GPS_RTS  (UART_PIN_NO_CHANGE)
#define GPS_CTS  (UART_PIN_NO_CHANGE)

/* Handlers externos */

 extern QueueHandle_t uart1_queue;
 extern QueueHandle_t Cola1;
 extern QueueHandle_t Datos_uart1;

static const char *TAG = "Config uart";

/* Esta funcion configura los uart*/
void  Configurar_UARTs(){

	uart_config_t  uart_config = {
	        .baud_rate = 115200,
	        .data_bits = UART_DATA_8_BITS,
	        .parity    = UART_PARITY_DISABLE,
	        .stop_bits = UART_STOP_BITS_1,
	        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
	        .source_clk = UART_SCLK_APB,
	    };

	//Configurar Uart1
    uart_driver_install(UART_NUM_1, BUF_SIZE, BUF_SIZE, 20, &uart1_queue, 0);
    uart_param_config(UART_NUM_1, &uart_config);
    //Install UART driver, and get the queue.
    //Configurar Uart0
    uart_driver_install(EX_UART_NUM, BUF_SIZE, BUF_SIZE, 20, NULL, 0);
    uart_param_config(EX_UART_NUM, &uart_config);
    //Install UART driver, and get the queue.
    //Set UART log level
    esp_log_level_set(TAG, ESP_LOG_INFO);
    //Set UART pins (using UART0 default pins ie no changes.)
    uart_set_pin(EX_UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    //Set UART pins (using UART1 default pins ie no changes.)
    uart_set_pin(UART_NUM_1, TX1, RX1, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);

    //Configurar Uart2
    uart_param_config(UART_NUM_2, &uart_config);
    uart_set_pin(UART_NUM_2, GPS_TXD, GPS_RXD, GPS_RTS, GPS_CTS);
    uart_driver_install(UART_NUM_2, BUF_SIZE * 2, 0, 0, NULL, 0); //BOGUS
}

/* Esta tarea recibe por eventos lo que llega al uart y lo manda por cola*/
  void uart1_event_task(void *pvParameters){
    uart_event_t event;
    struct TRAMA TX;

    for(;;) {
    	if(xQueueReceive(uart1_queue, (void * )&event, (portTickType)portMAX_DELAY)){
    		bzero(TX.dato,RD_BUF_SIZE);
    		switch(event.type) {
    		case UART_DATA:
    			TX.size=(uint16_t)event.size;
    			uart_read_bytes(UART_NUM_1, TX.dato, TX.size, portMAX_DELAY);
    			xQueueSend(Datos_uart1,&TX,0/portTICK_RATE_MS);
    			xQueueSend(Cola1,&TX,0/portTICK_RATE_MS);
    			break;
    		default:
    			ESP_LOGI(TAG, "uart event type: %d", event.type);
    			break;
    		}
    	}
    }
     vTaskDelete(NULL);
 }

  void echo_U1toU0(void *pvParameters){
 	//Esta funcion hace el echo del uart 1 al uart0
 	struct TRAMA RX;
 	  	  for(;;) {
 	  		xQueueReceive(Cola1,&RX,portMAX_DELAY);
 	  		uart_write_bytes(UART_NUM_0, (const char*)RX.dato, RX.size);

 	  	  	  }
 	    vTaskDelete(NULL);
 }




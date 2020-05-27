/* PPPoS Client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "dht.h"
#include <stdio.h>
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
#include "tracking.h"
#include "esp_sleep.h"
#include "driver/rtc_io.h"



/* prender el SIM800L*/
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


#define MEN_TXD  (GPIO_NUM_27)
#define MEN_RXD  (GPIO_NUM_26)
#define MEN_RTS  (UART_PIN_NO_CHANGE)
#define MEN_CTS  (UART_PIN_NO_CHANGE)



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


//uart echo

#define TX1 														27                                                                //
#define RX1 														26
#define LED 														13
#define EX_UART_NUM 												UART_NUM_0
#define PATTERN_CHR_NUM    											3        /*!< Set the number of consecutive and identical characters received by receiver which defines a UART pattern*/
#define RD_BUF_SIZE 												BUF_SIZE

static QueueHandle_t uart1_queue;
static QueueHandle_t Cola1;
static QueueHandle_t Datos_uart1;

struct TRAMA{
	uint8_t dato[BUF_SIZE];
	uint16_t size;
};



static void uart1_event_task(void *pvParameters)
{
   uart_event_t event;
   struct TRAMA TX;

    for(;;) {

       if(xQueueReceive(uart1_queue, (void * )&event, (portTickType)portMAX_DELAY)) {
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

static void echo_U1to0(void *pvParameters){
	struct TRAMA RX;
	  	  for(;;) {
	  		xQueueReceive(Cola1,&RX,portMAX_DELAY);
	  		uart_write_bytes(UART_NUM_0, (const char*)RX.dato, RX.size);

	  	  	  }
	    vTaskDelete(NULL);
}

static void  Tiempo_Espera(char* aux, uint8_t estado, uint16_t* tamano, uint8_t* error, portTickType tiempo)
{
	//Esta funcion se encarga de esperar el tiempo necesario para cada comando
	// Creo que se le puede anadir en el else la parte de los errores
	struct TRAMA buf;
    if(xQueueReceive(Datos_uart1, &buf, (portTickType) tiempo / portTICK_PERIOD_MS)) {
        memcpy(aux,buf.dato,BUF_SIZE);
        *tamano = buf.size;
        ESP_LOGW(TAG,"Size es: %d",buf.size);
        ESP_LOGW(TAG,"aux es: %s",buf.dato);
    } else {
    	printf("%d- Espere %d y nada",estado,(int) tiempo);
    	error++;
    }
}

static void  Prender_SIM800l()
{

	gpio_set_level(SIM800l_PWR, 1);
	gpio_set_level(SIM800l_RST, 1);
	gpio_set_level(SIM800l_PWR_KEY, 1);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	gpio_set_level(SIM800l_PWR_KEY, 0);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	gpio_set_level(SIM800l_PWR_KEY, 1);


	vTaskDelay(5000 / portTICK_PERIOD_MS);
//	ESP_LOGW("TAG","Ya espere 10");
}

static void  Enviar_mensaje(char* mensaje, uint8_t tamano)
{
	char aux[BUF_SIZE] = "32!";
	uint16_t size = 0;
	uint8_t error = 0;
	const char* finalSMSComand = "\x1A";
	ESP_LOGW("Mensaje","Mandare el mensaje");
	uart_write_bytes(UART_NUM_1,"AT+CMGS=\"+584242428865\"\r\n", 25);
	Tiempo_Espera(aux, 20,&size,error, t_CMGS);
	if(strncmp(aux,"\r\n>",3) == 0){
	    uart_write_bytes(UART_NUM_1,mensaje,tamano);
	    uart_write_bytes(UART_NUM_1,(const char*)finalSMSComand, 2);
	}else if(strncmp(aux,"\r\nCMS ERROR:",12) == 0 || strncmp(aux,"\r\nERROR",7) == 0){
		ESP_LOGE("Mensaje","No se pudo mandar el mensaje");
	}
}




//Escribir en la memoria flash
void set_form_flash_init( message_data_t *datos){
	esp_err_t err;
	nvs_handle_t ctrl_flash;
	err = nvs_open("storage",NVS_READWRITE,&ctrl_flash);
	if (err != ESP_OK) {
		printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
	}else{
		nvs_set_str(ctrl_flash,"Humedad",datos->Humedad);

		nvs_set_str(ctrl_flash,"Temperatura",datos->Temperatura);

		nvs_set_str(ctrl_flash,"Latitud",datos->Latitude);

		nvs_set_str(ctrl_flash,"Longitud",datos->Longitude);

		nvs_set_str(ctrl_flash,"Latitud_dir",datos->Latitude_dir);

		nvs_set_str(ctrl_flash,"Longitud_dir",datos->Longitude_dir);

		err = nvs_commit(ctrl_flash);
	}
	nvs_close(ctrl_flash);
}


void Mandar_mensaje(void *P)
{
	char message[318] = "Welcome to ESP32!";
	char aux[BUF_SIZE] = "";
	uint16_t size = 0;
	e_ATCOM ATCOM = 0;
	e_TEspera T_Espera;
	gps_data_t gps_data2;
	AM2301_data_t Thum2;
	message_data_t message_data;
	uint8_t flags_errores = 0;
	uint8_t flags2_errores = 0;
	uint8_t config_sim800 = 0;



	printf("Entre en mandar mensaje \r\n");
	for(;;){

		xEventGroupSync(event_group,BEGIN_TASK1,0xC0,portMAX_DELAY);
	   	xEventGroupClearBits(event_group, SYNC_BIT_TASK1);
	    xEventGroupClearBits(event_group, SYNC_BIT_TASK2);

	    ESP_LOGW(TAG, "Apagare y prendere\r\n");
	    /* Apagar el modulo por si esta prendido al entrar en la tarea */
	    uart_write_bytes(UART_NUM_1,"AT+CPOWD=1\r\n", 12);
	    vTaskDelay(2000 / portTICK_PERIOD_MS);
	    Prender_SIM800l();

	    ESP_LOGW(TAG, "Empezara la maquina de estados\r\n");
	    //Recibo la informacion de la temperatura
	    if(xQueueReceive(xQueue_temp,&Thum2,(portTickType) 1000 / portTICK_PERIOD_MS)) {
	    	sprintf(message_data.Humedad, "%f",Thum2.Prom_hum[Thum2.pos_temp-1]);
	    	sprintf(message_data.Temperatura, "%f",Thum2.Prom_temp[Thum2.pos_temp-1]);
	    } else {
	    	Thum2.error_temp = 1;
	    }

    	//Recibo la informacion del gps
	    if(xQueueReceive(xQueue_temp,&Thum2,(portTickType) 1000 / portTICK_PERIOD_MS)) {
	    	sprintf(message_data.Latitude, "%f",gps_data2.latitude_prom);
	    	sprintf(message_data.Longitude, "%f",gps_data2.longitude_prom);
	    	sprintf(message_data.Latitude_dir, "%s",gps_data2.latitude_direct);
	    	sprintf(message_data.Longitude_dir, "%s",gps_data2.longitude_direct);
	    } else {
	    	gps_data2.error_gps = 1;
	    }


    	//Verifico si se supero el limite de temperatura o se abrio la puerta para guardar la informacion
    	//en la memoria flash
    	if (limite_b == 1 || puerta_b == 1){
        		set_form_flash_init(&message_data);
        	}
    	ESP_LOGW(TAG, "Empezara la maquina de estados\r\n");
    /* Para mandar mensajes con menuconfig se puede configurar el numero que recibira el mensaje
     y el mensaje va a ser el la variable message, recordando que tiene un limite de caracteres
     * */
    while(config_sim800 == 0){
    	switch (ATCOM){
    	case CMGF:
            //Para configurar el formato de los mensajes
            uart_write_bytes(UART_NUM_1,"AT+CMGF=1\r\n", 11);
            ESP_LOGW(TAG, "Cmgf activo \r\n");
            Tiempo_Espera(aux, ATCOM,&size,&flags_errores,t_CMGF);
            if(strncmp(aux,"\r\nOK",4) == 0){
            	ATCOM++;
                ESP_LOGW(TAG,"Aumentando ATCOM");
                flags_errores = 0;
            }else if(strncmp(aux,"\r\nERROR",7) == 0){
            	ESP_LOGE(TAG,"CMGF- Dio Error");
            	flags_errores++;
            	if (flags_errores >= 3){
            		ATCOM = CPOWD;
            	}
            }
            bzero(aux, BUF_SIZE);
            size = 0;
    	break;
    	case CPAS:
    		// Verificar que se encuentra conectado a la radio base
    		uart_write_bytes(UART_NUM_1,"AT+CPAS\r\n", 9);
	        ESP_LOGW(TAG, "Mande CPAS \r\n");
	        Tiempo_Espera(aux, ATCOM,&size,&flags_errores,t_CPAS);
	        if(strncmp(aux,"\r\n+CPAS: 0",10) == 0){
	        	ESP_LOGE(TAG,"CPAS- Respondio bien");
	        	config_sim800 = 1;
           		flags_errores = 0;
	        }else if(strncmp(aux,"\r\nERROR",7) == 0){
	        	ESP_LOGE(TAG,"CPAS- Dio Error");
	        	flags_errores++;
	        	if (flags_errores >= 3){
	        		ATCOM = CPOWD;
	        	}
           }
           bzero(aux, BUF_SIZE);
           size = 0;
        break;
       	case CPOWD:
        	//Para apagar el sim800l
            ESP_LOGW(TAG, "Apagar \r\n");
            uart_write_bytes(UART_NUM_1,"AT+CPOWD=1\r\n", 12);
            Tiempo_Espera(aux, ATCOM,&size,&flags_errores,t_CPOWD);
            if(strncmp(aux,"\r\nNORMAL POWER DOWN",19) == 0){
               	ESP_LOGW(TAG,"Se apago el modulo SIM800L");
            	if (flags_errores >= 3){
            		flags_errores = 0;
            		Prender_SIM800l();
	            	vTaskDelay(5000 / portTICK_PERIOD_MS);
	            	ATCOM = CMGF;
            	} else{
               	config_sim800 = 1;
               	flags_errores = 0;
            	}
            }else {
            	ESP_LOGE(TAG,"CPOWD- Dio Error");
            	flags_errores++;
            	if (flags_errores >= 3){
            		flags2_errores = 1;
            		flags_errores = 0;
            	}
            }
            bzero(aux, BUF_SIZE);
            size = 0;
        break;
    	}
    	ESP_LOGW(TAG,"Final del while");
    	vTaskDelay(1000 / portTICK_PERIOD_MS);
    	//verificacion por si dio error en el envio del apagado
    	//Esto puede ser usado tambien en otros casos para salir del ciclo
    	if ( flags2_errores == 1){
    		flags2_errores = 0;
    		break;
    	}
    }
    	ATCOM = CMGF;
    	config_sim800 = 0;


    //Se verifica si se logro medir la temperatura y se manda el mensaje correspondiente

    if (Thum2.error_temp == 0){
    	switch(limite_b){
    	case 0:
    		sprintf(message,"La humedad es: %.1f %% y la temperatura es: %.1f C",Thum2.Prom_hum[Thum2.pos_temp-1],Thum2.Prom_temp[Thum2.pos_temp-1]);
    		Enviar_mensaje(message,48);
    		ESP_LOGI(TAG, "Send send message [%s] ok", message);
    	break;
    	case 1:
    		sprintf(message,"La temperatura se salio de los limites. La humedad es: %.1f  %% y la temperatura es: %.1f C",Thum2.Prom_hum[Thum2.pos_temp-1],Thum2.Prom_temp[Thum2.pos_temp-1]);
    		Enviar_mensaje(message,89);
    		ESP_LOGI(TAG, "Send send message [%s] ok", message);
    		if (limite_a == 0){
			limite_b = 0;
    		}
    	break;
    	}
    } else {
    	Thum.error_temp = 0;
    	 sprintf(message,"No se logro medir la temepratura. Revisar las conexiones.");
    	 Enviar_mensaje(message,57);
    	 ESP_LOGI(TAG, "Send send message [%s] ok", message);
    }

    switch (puerta_b){
    case P_cerrada:
    // Verifico si no hubo error al conectarse a al modulo GPS
        if (gps_data2.error_gps == 0){
        	//Verifico si se conecto a la red GPS viendo si devolvio que es el a;o 2020
        	if (gps_data2.year == 20){
			    sprintf(message,"La latitud es: %.4f %s y la longitud es: %.4f %s",gps_data2.latitude_prom,gps_data2.latitude_direct,gps_data2.longitude_prom,gps_data2.longitude_direct);
		    	Enviar_mensaje(message,87);
			    ESP_LOGI(TAG, "Send send message [%s] ok", message);

        		sprintf(message,"Las medidas se realizaron el %d de %s de 20%d a las %d horas con %d minutos y %d segundos",gps_data2.day,gps_data2.mes,gps_data2.year,gps_data2.hour,gps_data2.minute,gps_data2.second);
        		Enviar_mensaje(message,112);
        		ESP_LOGI(TAG, "Send send message [%s] ok", message);
        	} else{

        		 sprintf(message,"No se logro conectar a la red GPS.");
        		 Enviar_mensaje(message,34);
        		 ESP_LOGI(TAG, "Send send message [%s] ok", message);
        	}
        } else{
  			gps_data.error_gps = 0;
    		sprintf(message,"No se logro conectar con el modulo GPS.");
    		Enviar_mensaje(message,39);
    		ESP_LOGI(TAG, "Send send message [%s] ok", message);
        }
    break;
    case P_abierta:
    	// Verifico si no hubo error al conectarse a al modulo GPS
    	if (gps_data2.error_gps == 0){
    	//Verifico si se conecto a la red GPS viendo si devolvio que es el a;o 2020
    		if (gps_data2.year == 20){

    			 sprintf(message,"La puerta fue abierta. La latitud es: %.4f %s y la longitud es: %.4f %s",gps_data2.latitude_prom,gps_data2.latitude_direct,gps_data2.longitude_prom,gps_data2.longitude_direct);
    			 Enviar_mensaje(message,87);
    			 ESP_LOGI(TAG, "Send send message [%s] ok", message);
    			if (puerta_a == 0){
    				puerta_b = 0;
    				puerta_c = 0;
    			}

    	         sprintf(message,"Las medidas se realizaron el %d de %s de 20%d a las %d horas con %d minutos y %d segundos",gps_data2.day,gps_data2.mes,gps_data2.year,gps_data2.hour,gps_data2.minute,gps_data2.second);
    	         Enviar_mensaje(message,112);
    	         ESP_LOGI(TAG, "Send send message [%s] ok", message);
    	    } else{

    	    	 sprintf(message,"La puerta se abrio, pero no se logro conectar a la red GPS.");
    	    	 Enviar_mensaje(message,59);
    	    	 ESP_LOGI(TAG, "Send send message [%s] ok", message);
    			if (puerta_a == 0){
    				puerta_b = 0;
    				puerta_c = 0;
    			}
    	    }
    	} else{
			gps_data.error_gps = 0;
			sprintf(message,"La puerta se abrio, pero no se logro conectar con el modulo GPS.");
			Enviar_mensaje(message,64);
			ESP_LOGI(TAG, "Send send message [%s] ok", message);
			if (puerta_a == 0){
				puerta_b = 0;
				puerta_c = 0;
			}
    	}
    break;
    }



    /* Power down module */
    ESP_LOGI(TAG, "Power down");
    uart_write_bytes(UART_NUM_1,"AT+CPOWD=1\r\n", 12);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
	}
}




void app_main(void)
{

	//nvs_flash_init();

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

		Cola1= xQueueCreate(1, sizeof(struct TRAMA));
		Datos_uart1 = xQueueCreate(1, sizeof(struct TRAMA));

	    uart_config_t uart_config = {
	        .baud_rate = 115200,
	        .data_bits = UART_DATA_8_BITS,
	        .parity    = UART_PARITY_DISABLE,
	        .stop_bits = UART_STOP_BITS_1,
	        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
	        .source_clk = UART_SCLK_APB,
	    };


	    uart_driver_install(UART_NUM_1, BUF_SIZE, BUF_SIZE, 20, &uart1_queue, 0);
	    uart_param_config(UART_NUM_1, &uart_config);
	    //Install UART driver, and get the queue.
	    uart_driver_install(EX_UART_NUM, BUF_SIZE, BUF_SIZE, 20, NULL, 0);
	    uart_param_config(EX_UART_NUM, &uart_config);
	    //Install UART driver, and get the queue.
	    //Set UART log level
	    esp_log_level_set(TAG, ESP_LOG_INFO);
	    //Set UART pins (using UART0 default pins ie no changes.)
	    uart_set_pin(EX_UART_NUM, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
	    //Set UART pins (using UART1 default pins ie no changes.)
	    uart_set_pin(UART_NUM_1, TX1, RX1, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);


	     //Create a task to handler UART event from ISR
	    // xTaskCreate(task_test, "tarea de prueba", 3*1024, NULL, 2, &xTask2Handle);
	    xTaskCreate(echo_U1to0, "Echo a uart0", 4*1024, NULL, 2, NULL);
	    xTaskCreate(uart1_event_task, "uart1_event_task", 10*2048, NULL, 1, NULL);




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


	//Creo el grupo de eventos y las colas
	event_group = xEventGroupCreate();
	xQueue_temp = xQueueCreate(1, sizeof(AM2301_data_t));
	xQueue_gps = xQueueCreate(1, sizeof(gps_data_t));


	xTaskCreatePinnedToCore(&TareaDHT, "TareaDHT", 1024*4, NULL, 5, NULL,0);
	xTaskCreatePinnedToCore(&echo_task, "uart_echo_task", 1024*8, NULL, 5, NULL,1);
	xTaskCreatePinnedToCore(&Mandar_mensaje, "Mandar mensaje2", 1024*10, NULL, 6, NULL,1);
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

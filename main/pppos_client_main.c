/*

*/
#include "dht.h"
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "sim800.h"
#include "bg96.h"
#include "nvs_flash.h"
#include "tracking.h"
//#include "esp_sleep.h"
//#include "driver/rtc_io.h"



/* Configuracion de pines del SIM800L*/

#define SIM800l_PWR_KEY (4)
#define SIM800l_PWR (23)
#define SIM800l_RST (5)

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

//Varaible para saber si dio error PDP_deact

uint8_t pdp_deact = 0;


#define MEN_TXD  (GPIO_NUM_27)
#define MEN_RXD  (GPIO_NUM_26)
#define MEN_RTS  (UART_PIN_NO_CHANGE)
#define MEN_CTS  (UART_PIN_NO_CHANGE)


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
	//Esta tarea recibe por eventos lo que llega al uart y lo manda por cola
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
	//Esta funcion hace el echo del uart 1 al uart0
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
	struct TRAMA buf;
    if(xQueueReceive(Datos_uart1, &buf, (portTickType) tiempo / portTICK_PERIOD_MS)) {
    //	fflush(UART_NUM_1);
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
	// Esta funcion prende el modulo sim800 y le da un tiempo para que se conecte a la radiobase
	gpio_set_level(SIM800l_PWR, 1);
	gpio_set_level(SIM800l_RST, 1);
	gpio_set_level(SIM800l_PWR_KEY, 1);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	gpio_set_level(SIM800l_PWR_KEY, 0);
	vTaskDelay(1000 / portTICK_PERIOD_MS);
	gpio_set_level(SIM800l_PWR_KEY, 1);


	vTaskDelay(7000 / portTICK_PERIOD_MS);
//	ESP_LOGW("TAG","Ya espere 10");
}

static void  Envio_mensaje(char* mensaje, uint8_t tamano)
{
	//Esta funcion enviara el mensaje que se le pase
	char aux[BUF_SIZE] = "32!";
	uint16_t size = 0;
	uint8_t error = 0, jail = 0;
	const char* finalSMSComand = "\x1A";

	//Se manda el comando AT para comenzar el envio del mensaje
	ESP_LOGW("Mensaje","Mandare el mensaje");
	uart_write_bytes(UART_NUM_1,"AT+CMGS=\"+584242428865\"\r\n", 25);

	//Se espera la respuesta y luego se compara a ver si es la esperada
	//La respuesta es "inmediata" asi que no deberia hacer falta ponerlo en un ciclo
	Tiempo_Espera(aux, 20,&size,error, t_CMGS);
    vTaskDelay(300 / portTICK_PERIOD_MS);
	if(strncmp(aux,"\r\n>",3) == 0){
		uart_write_bytes(UART_NUM_1,mensaje,tamano);
		uart_write_bytes(UART_NUM_1,(const char*)finalSMSComand, 2);
	}else if(strncmp(aux,"\r\nCMS ERROR:",12) == 0 || strncmp(aux,"\r\nERROR",7) == 0){
		ESP_LOGE("Mensaje","No se pudo mandar el mensaje");
	}else if (error >= 1){
		jail = 1;
	}
    ESP_LOGW(TAG, "Mensaje2 \r\n");
    //Aqui espero la respuesta al envio de mensaje para poder terminar la tarea y no molestar
    // en el siguiente mensaje
    while (jail == 0){
    	Tiempo_Espera(aux, 21,&size,error,t_CMGS);
    	if(strncmp(aux,"\r\n+CMGS:",8) == 0){
    		jail = 1;
    	}else if(strncmp(aux,"\r\nCMS ERROR:",12) == 0 || strncmp(aux,"\r\nERROR",7) == 0){
    		ESP_LOGE(TAG,"21- Dio Error");
    		error++;
    		if (error >= 1){
    			ESP_LOGE(TAG,"21- No se mando el mensaje Error");
    			break;
    		}
    	}
    vTaskDelay(700 / portTICK_PERIOD_MS);
    }

	vTaskDelay(1000 / portTICK_PERIOD_MS);
}

static e_ATCOM  Configurar_GSM(e_ATCOM ATCOM)
{
	//Esta funcion de configurar el sim800 para enviar mensajes de texto
	ATCOM = CMGF;
	char aux[BUF_SIZE] = "!";
	uint16_t size = 0;
	uint8_t error = 0, config = 0;
	uint8_t flags_errores = 0, flags2_errores = 0, flags3_errores = 0;
	//Cuando leia config  daba un valor incorrecto, asi que lo paso de nuevo aca, creo
	//que se puede borrar config de afuera de esta funcion

    while(config == 0){
    	switch (ATCOM){
    	case CMGF:
            //Para configurar el formato de los mensajes
            ESP_LOGW(TAG, "Enviara CMGF \r\n");
            uart_write_bytes(UART_NUM_1,"AT+CMGF=1\r\n", 11);
            vTaskDelay(500 / portTICK_PERIOD_MS);
            Tiempo_Espera(aux, ATCOM,&size,&flags_errores,t_CMGF);
            if(strncmp(aux,"\r\nOK",4) == 0){
            	ATCOM++;
                ESP_LOGW(TAG,"Aumentando ATCOM");
                flags_errores = 0;
            }else if(strncmp(aux,"\r\nERROR",7) == 0){
            	ESP_LOGE(TAG,"CMGF- Dio Error");
            	flags_errores++;
            	if (flags_errores >= 5){
            		ATCOM = CPOWD;
            	}
            } else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
            	ESP_LOGE(TAG,"CMGF- PDP DEACT");
            	flags_errores++;
            	pdp_deact = 1;
            	ATCOM = CPOWD;
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
	        	config = 1;
           		flags_errores = 0;
	        }else if(strncmp(aux,"\r\nERROR",7) == 0){
	        	ESP_LOGE(TAG,"CPAS- Dio Error");
	        	flags_errores++;
	        	if (flags_errores >= 3){
	        		ATCOM = CPOWD;
	        	}
           } else if(strncmp(aux,"\r\n+CPAS: 2",10) == 0 || strncmp(aux,"\r\n+CPAS: 1",10) == 0){
        	   flags3_errores++;
	        	if (flags3_errores >= 10){
	        		ATCOM = CPOWD;
	        	}
           } else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
           	ESP_LOGE(TAG,"CPAS- PDP DEACT");
           	flags_errores++;
           	pdp_deact = 1;
           	ATCOM = CPOWD;
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
	            	config = 1;
            	} else{
               	config = 1;
               	flags_errores = 0;
            	}
            }else {
            	ESP_LOGE(TAG,"CPOWD- Dio Error");
            	flags_errores++;
            	if (flags_errores >= 3){
            		flags2_errores = 1;
            		flags_errores = 0;
            		config = 1;
            	}
            }
            bzero(aux, BUF_SIZE);
            size = 0;
        break;
    	}
    	ESP_LOGW(TAG,"Final del while");
    	vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
	ESP_LOGW(TAG,"Termine de configurar para GSM");
	return(ATCOM);
}

static e_ATCOM2  Configurar_GPRS(e_ATCOM2 ATCOM)
{
	//Esta funcion de configurar el sim800 para enviar datos GPRS
	uint8_t a = 0;
	ATCOM = CFUN;
	char aux[BUF_SIZE] = "32!";
	uint16_t size = 0;
	uint8_t error = 0, jail = 0;
	uint8_t flags_errores = 0, flags2_errores = 0;

	while (a == 0){

    	switch(ATCOM){
    	case CFUN:
            // Se activan las funcionalidades
            ESP_LOGW(TAG,"Mandara CFUN");
            uart_write_bytes(UART_NUM_1,"AT+CFUN=1\r\n", 11);
            //Con este delay se evitan errores despues
            //Por alguna razon la primera vez que se envia este comando nunca recibe la
            //respuesta correcta
            vTaskDelay(500 / portTICK_PERIOD_MS);
            Tiempo_Espera(aux, ATCOM+30,&size,error, t_CFUN);
            if(strncmp(aux,"\r\nOK",4) == 0){
            	ATCOM++;
            	ESP_LOGW(TAG,"Aumentando ATCOM");
            	flags_errores = 0;
            }else if(strncmp(aux,"\r\nCME ERROR:",12) == 0 || strncmp(aux,"\r\nERROR",7) == 0){
            	ESP_LOGE(TAG,"CFUN- Dio Error");
            	flags_errores++;
            	if (flags_errores >= 3){
            		ATCOM = CPOWD2;
            	}
            } else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
            	ESP_LOGE(TAG,"CFUN- PDP DEACT");
            	flags_errores++;
            	pdp_deact = 1;
            	ATCOM = CPOWD2;
            }
            bzero(aux, BUF_SIZE);
            size = 0;
    	break;
    	case CSTT:
            //Para conectarse a la red de Movistar
            ESP_LOGW(TAG,"Mando CSTT");
            uart_write_bytes(UART_NUM_1,"AT+CSTT=\"internet.movistar.ve\",\"\",\"\"\r\n", 39);
            ESP_LOGW(TAG, "Conectandose a movistar \r\n");
            Tiempo_Espera(aux,ATCOM+30,&size,error,t_CSST);
            if(strncmp(aux,"\r\nOK",4) == 0){
            	ATCOM++;
            	ESP_LOGW(TAG,"Aumentando ATCOM");
            	flags_errores = 0;
            	vTaskDelay(10000 / portTICK_PERIOD_MS);
            } else if(strncmp(aux,"\r\nERROR",7) == 0 ){
            	ESP_LOGE(TAG,"CSTT- Dio error");
            	flags_errores++;
            	if (flags_errores >= 3){
            		ATCOM = CPOWD2;
            	}
            } else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
            	ESP_LOGE(TAG,"CMGF- PDP DEACT");
            	flags_errores++;
            	pdp_deact = 1;
            	ATCOM = CPOWD2;
            }
            bzero(aux, BUF_SIZE);
            size = 0;
    	break;
    	case CIICR:
            // Para activar la conexion inalambrica por GPRS
    	    ESP_LOGW(TAG, "Mando Ciirc\r\n");
    		uart_write_bytes(UART_NUM_1,"AT+CIICR\r\n", 10);
    	    ESP_LOGW(TAG, "Ciirc activando \r\n");
    	    Tiempo_Espera(aux, ATCOM+30,&size,error,t_CIICR);
            if(strncmp(aux,"\r\nOK",4) == 0){
            	ATCOM++;
            	ESP_LOGW(TAG,"Aumentando ATCOM");
            	flags_errores = 0;
            }else if(strncmp(aux,"\r\nERROR",7) == 0 ){
            	ESP_LOGE(TAG,"CIICR- Dio error");
            	flags_errores++;
            	if (flags_errores >= 2){
            		ATCOM = CPOWD2;
            	}
            }else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
            	ESP_LOGE(TAG,"CIICR- PDP DEACT");
            	flags_errores++;
            	pdp_deact = 1;
            	ATCOM = CPOWD2;
            }
            bzero(aux, BUF_SIZE);
            size = 0;
    	break;
    	case CGREG:
    		//Verificando que este conectado al GPRS
    		ESP_LOGW(TAG, "Mando CGREG\r\n");
    		uart_write_bytes(UART_NUM_1,"AT+CGREG?\r\n", 11);
    		Tiempo_Espera(aux, ATCOM+30,&size,error,t_CGREG);
            if(strncmp(aux,"\r\n+CGREG: 0,1",13) == 0){
            	ATCOM++;
            	ESP_LOGW(TAG,"Aumentando ATCOM");
            	flags_errores = 0;
            }else if(strncmp(aux,"\r\nCME ERROR:",12) == 0 || strncmp(aux,"\r\nERROR",7) == 0){
            	ESP_LOGE(TAG,"CGREG- Dio Error");
            	flags_errores++;
            	if (flags_errores >= 3){
            		ATCOM = CPOWD2;
            	}
            } else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
            	ESP_LOGE(TAG,"CGREG- PDP DEACT");
            	flags_errores++;
            	pdp_deact = 1;
            	ATCOM = CPOWD2;
            }
            bzero(aux, BUF_SIZE);
            size = 0;
    	break;
    	case CIFSR:
		       // Para pedir la ip asignada
		    uart_write_bytes(UART_NUM_1,"AT+CIFSR\r\n", 10);
		    ESP_LOGW(TAG, "Pidiendo IP \r\n");
		    Tiempo_Espera(aux, ATCOM+30,&size,error,t_CIFSR);
		    //BUsco si lo que llego al uart tiene un . para ver si respondio la ip
		    char* ip = strstr(aux,".");
		    if (ip == NULL || strncmp(aux,"\r\nERROR",7) == 0){
		    ESP_LOGE(TAG,"CIFSR- Dio Error");
            flags_errores++;
            if (flags_errores >= 3){
            		ATCOM = CPOWD2;
            }
            } else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
            	ESP_LOGE(TAG,"COPWD2- PDP DEACT");
            	flags_errores++;
            	pdp_deact = 1;
            	ATCOM = CPOWD2;
            } else {
            	a = 1;
            	flags_errores = 0;
            }
            bzero(aux, BUF_SIZE);
            size = 0;
         break;
    	case CPOWD2:
    		//Para apagar el sim800l
            ESP_LOGW(TAG, "Apagar \r\n");
            uart_write_bytes(UART_NUM_1,"AT+CPOWD=1\r\n", 12);
            Tiempo_Espera(aux, ATCOM+30,&size,error,t_CPOWD);
            if(strncmp(aux,"\r\nNORMAL POWER DOWN",19) == 0){
            	ESP_LOGW(TAG,"Se apago el modulo SIM800L");
            	flags_errores = 0;
            }  else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
            	ESP_LOGE(TAG,"CPOWD2- PDP DEACT");
            	flags_errores++;
            	if (flags_errores >= 3){
             		flags_errores = 0;
             		a = 1;
            	}
            } else {
            	ESP_LOGE(TAG,"CPOWD2- Dio Error");
            	flags_errores++;
            	if (flags_errores >= 3){
             		flags_errores = 0;
             		a = 1;
            	}
            }
            bzero(aux, BUF_SIZE);
            size = 0;
    	break;
	}
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}
	return ATCOM;
}

static void  Envio_GPRS_temp(AM2301_data_t* Thum2){
	//Esta funcion envia los datos a thingspeak

	char message[318] = "Welcome to ESP32!";
	char aux[BUF_SIZE] = "";
	uint16_t size = 0 , a = 0;
//	e_TEspera T_Espera;
	uint8_t error = 0;
	e_ATCOM3 ATCOM3 = 0;

	ESP_LOGE("DATOS A ENVIAR", "ENVIO %d",(int) Thum2->Prom_temp[Thum2->pos_temp-1] );
    ESP_LOGE("DATOS A ENVIAR", "La posicion es %d",Thum2->pos_temp-1);
 //   ESP_LOGE("DATOS A ENVIAR", "El promedio es %d",(int)promedio);
    vTaskDelay(5000 / portTICK_PERIOD_MS);


while (a == 0){
	switch (ATCOM3){
    case CIPSTART:
    	//Para inciar la comunicacion con thingspeak
    	ESP_LOGW(TAG, "CIPSTART1\r\n");
    	uart_write_bytes(UART_NUM_1,"AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",80\r\n", 43);
    	Tiempo_Espera(aux, ATCOM3+40,&size,error,t_CIPSTART);
    	if(strncmp(aux,"\r\nCONNECT OK",12) == 0 ||
    			strncmp(aux,"\r\nALREADY CONNECTED",7) == 0 ||
    			strncmp(aux,"\r\nOK",4) == 0){
                ATCOM3++;
                ESP_LOGW(TAG,"Aumentando ATCOM");
                error = 0;
        }else if(strncmp(aux,"\r\nERROR",7) == 0){
        	ESP_LOGE(TAG,"CIPSTART- TEMP Dio Error");
            error++;
            if (error >= 2){
            	ATCOM3 = CPOWD3;
            }
        } else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
        	ESP_LOGE(TAG,"CIPSTART- TEMP PDP DEACT");
        	error++;
        	pdp_deact = 1;
        	ATCOM3 = CPOWD3;
        }
        bzero(aux, BUF_SIZE);
        size = 0;
	break;
    case CIPSTART2:
    	//Para esperar el connect ok
    	ESP_LOGW(TAG, "Connect ok \r\n");
        	Tiempo_Espera(aux, ATCOM3+40,&size,error,t_CIPSTART);
        	if(strncmp(aux,"\r\nCONNECT OK",12) == 0 ||
        		strncmp(aux,"\r\nALREADY CONNECTED",7) == 0){
        		ATCOM3++;
        		ESP_LOGW(TAG,"Aumentando ATCOM");
        		error = 0;
        		vTaskDelay(2000 / portTICK_PERIOD_MS);
        	}else if(strncmp(aux,"\r\nCMS ERROR:",12) == 0 || strncmp(aux,"\r\nERROR",7) == 0){
        		ESP_LOGE(TAG,"CIPSTART2- TEMP Dio Error");
        		error++;
        		if (error >= 2){
        			ATCOM3 = CPOWD3;
        		}
        	}else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
            	ESP_LOGE(TAG,"CIPSTART2- TEMP PDP DEACT");
            	error++;
            	pdp_deact = 1;
            	ATCOM3 = CPOWD3;
            }
        bzero(aux, BUF_SIZE);
        size = 0;
        break;
        case CIPSEND:
        	//Para mandar datos a thingspeak
            ESP_LOGW(TAG, "CIPSEND\r\n");
            uart_write_bytes(UART_NUM_1,"AT+CIPSEND=75\r\n", 15);
            Tiempo_Espera(aux,40,&size,error,t_CIPSEND);
            vTaskDelay(500 / portTICK_PERIOD_MS);
            if(strncmp(aux,"\r\n>",3) == 0){
            	//para la temperatura y humedad con lenght de 75
            	sprintf(message,"GET https://api.thingspeak.com/update?api_key=VYU3746VFOJQ2POH&field1=%d\r\n",(int) Thum2->Prom_temp[Thum2->pos_temp-1]);
            	uart_write_bytes(UART_NUM_1,message,75);
                ESP_LOGW(TAG,"Temperatura enviada");
                error = 0;
                ATCOM3++;
        	}else if(strncmp(aux,"\r\nCMS ERROR:",12) == 0 || strncmp(aux,"\r\nERROR",7) == 0){
        		ESP_LOGE(TAG,"CIPSEND- TEMP Dio Error");
        		error++;
        		if (error >= 2){
        			ATCOM3 = CPOWD3;
        		}
        	} else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
            	ESP_LOGE(TAG,"CIPSEND- TEMP PDP DEACT");
            	error++;
            	pdp_deact = 1;
            	ATCOM3 = CPOWD3;
            }
        	bzero(aux, BUF_SIZE);
        	size = 0;
        	break;
        	case CIPSEND2:
        		//Para eseprar la respuesta
        	    ESP_LOGW(TAG, "CIPSEND2\r\n");
        	    Tiempo_Espera(aux, 42,&size,error,t_CIPSEND);
        	    if(strncmp(aux,"\r\nCLOSED",8) == 0){
        	        ESP_LOGW(TAG,"Socket cerrado");
        	        a = 1;
                }else if(strncmp(aux,"\r\nCMS ERROR:",12) == 0 || strncmp(aux,"\r\nERROR",7) == 0){
                    ESP_LOGE(TAG,"CIPSEND2- TEMP Dio error");
                    error++;
                    if (error >= 2){
                    	ATCOM3 = CPOWD3;
                    }
                } else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
                	ESP_LOGE(TAG,"CIPSEND2- TEMP PDP DEACT");
                	error++;
                	pdp_deact = 1;
                	ATCOM3 = CPOWD3;
                }
                bzero(aux, BUF_SIZE);
                size = 0;
            break;
        	case CPOWD3:
        		//Para apagar el sim800l
                ESP_LOGW(TAG, "Apagar \r\n");
                uart_write_bytes(UART_NUM_1,"AT+CPOWD=1\r\n", 12);
                Tiempo_Espera(aux, ATCOM3+30,&size,error,t_CPOWD);
                if(strncmp(aux,"\r\nNORMAL POWER DOWN",19) == 0){
             	   ESP_LOGW(TAG,"Se apago el modulo SIM800L");
             	   error = 0;
             	   a = 1;
                } else {
             	   ESP_LOGE(TAG,"CPOWD3- Dio Error");
             	   error++;
             	   if (error >= 3){
             		   error = 0;
             		   a = 1;
             	   }
                }
                bzero(aux, BUF_SIZE);
                size = 0;
        	break;
	}
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}
}

static void  Envio_GPRS_hum(AM2301_data_t* Thum2){
	//Esta funcion envia los datos a thingspeak

	char message[318] = "Welcome to ESP32!";
	char aux[BUF_SIZE] = "";
	uint16_t size = 0, a = 0;
	uint8_t error = 0;
	e_ATCOM3 ATCOM3 = 0;

	while (a == 0){
	switch (ATCOM3){
    case CIPSTART:
    	//Para inciar la comunicacion con thingspeak
    	ESP_LOGW(TAG, "CIPSTART 1\r\n");
    	uart_write_bytes(UART_NUM_1,"AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",80\r\n", 43);
    	Tiempo_Espera(aux, ATCOM3+40,&size,error,t_CIPSTART);
    	if(strncmp(aux,"\r\nCONNECT OK",12) == 0 ||
    		strncmp(aux,"\r\nALREADY CONNECTED",7) == 0 ||
    		strncmp(aux,"\r\nOK",4) == 0){
            ATCOM3++;
            ESP_LOGW(TAG,"Aumentando ATCOM");
            error = 0;
        }else if(strncmp(aux,"\r\nERROR",7) == 0){
        	ESP_LOGE(TAG,"CIPSTART- TEMP Dio Error");
            error++;
            if (error >= 2){
            	ATCOM3 = CPOWD3;
            }
        } else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
        	ESP_LOGE(TAG,"CIPSTART- HUM PDP DEACT");
        	error++;
        	pdp_deact = 1;
        	ATCOM3 = CPOWD3;
        }
        bzero(aux, BUF_SIZE);
        size = 0;
	break;
    case CIPSTART2:
    	//Para esperar el connect ok
    	ESP_LOGW(TAG, "Connect ok \r\n");
        Tiempo_Espera(aux, ATCOM3+40,&size,error,t_CIPSTART);
        if(strncmp(aux,"\r\nCONNECT OK",12) == 0 ||
        	strncmp(aux,"\r\nALREADY CONNECTED",7) == 0){
        	ATCOM3++;
        	ESP_LOGW(TAG,"Aumentando ATCOM");
        	error = 0;
        	vTaskDelay(2000 / portTICK_PERIOD_MS);
        }else if(strncmp(aux,"\r\nCMS ERROR:",12) == 0 || strncmp(aux,"\r\nERROR",7) == 0){
        	ESP_LOGE(TAG,"CIPSTART2- TEMP Dio Error");
        	error++;
        	if (error >= 2){
        		ATCOM3 = CPOWD3;
        	}
        } else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
        	ESP_LOGE(TAG,"CIPSTART2- HUM PDP DEACT");
        	error++;
        	pdp_deact = 1;
        	ATCOM3 = CPOWD3;
        }
        bzero(aux, BUF_SIZE);
        size = 0;
        break;
        case CIPSEND:
        	//Para mandar datos a thingspeak
            ESP_LOGW(TAG, "CIPSEND\r\n");
            uart_write_bytes(UART_NUM_1,"AT+CIPSEND=74\r\n", 15);
            Tiempo_Espera(aux,40,&size,error,t_CIPSEND);
            vTaskDelay(300 / portTICK_PERIOD_MS);
            if(strncmp(aux,"\r\n>",3) == 0){
            	//para la temperatura y humedad con lenght de 75
            	sprintf(message,"GET https://api.thingspeak.com/update?api_key=VYU3746VFOJQ2POH&field2=%d\r\n",(int) Thum2->Prom_hum[Thum2->pos_temp-1]);
            	uart_write_bytes(UART_NUM_1,message,74);
                ESP_LOGW(TAG,"Humedad enviada");
                error = 0;
                ATCOM3++;
        	}else if(strncmp(aux,"\r\nCMS ERROR:",12) == 0 || strncmp(aux,"\r\nERROR",7) == 0){
        		ESP_LOGE(TAG,"CIPSEND- TEMP Dio Error");
        		error++;
        		if (error >= 2){
        			ATCOM3 = CPOWD3;
        		}
        	} else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
            	ESP_LOGE(TAG,"CIPSEND- HUM PDP DEACT");
            	error++;
            	pdp_deact = 1;
            	ATCOM3 = CPOWD3;
            }
        	bzero(aux, BUF_SIZE);
        	size = 0;
        	break;
        	case CIPSEND2:
        		//Para eseprar la respuesta
        	    ESP_LOGW(TAG, "CIPSEND2\r\n");
        	    Tiempo_Espera(aux, 42,&size,error,t_CIPSEND);
        	    if(strncmp(aux,"\r\nCLOSED",8) == 0){
        	        ESP_LOGW(TAG,"Socket cerrado");
        	        a = 1;
                }else if(strncmp(aux,"\r\nCMS ERROR:",12) == 0 || strncmp(aux,"\r\nERROR",7) == 0){
                    ESP_LOGE(TAG,"CIPSEND2- TEMP Dio error");
                    error++;
                    if (error >= 2){
                    	ATCOM3 = CPOWD3;
                    }
                } else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
                	ESP_LOGE(TAG,"CIPSEND2- HUM PDP DEACT");
                	error++;
                	pdp_deact = 1;
                	ATCOM3 = CPOWD3;
                }
                bzero(aux, BUF_SIZE);
                size = 0;
            break;
        	case CPOWD3:
        		//Para apagar el sim800l
               ESP_LOGW(TAG, "Apagar \r\n");
               uart_write_bytes(UART_NUM_1,"AT+CPOWD=1\r\n", 12);
               Tiempo_Espera(aux, ATCOM3+30,&size,error,t_CPOWD);
               if(strncmp(aux,"\r\nNORMAL POWER DOWN",19) == 0){
            	   ESP_LOGW(TAG,"Se apago el modulo SIM800L");
            	   error = 0;
            	   a = 1;
               } else {
            	   ESP_LOGE(TAG,"CPOWD3- Dio Error");
            	   error++;
            	   if (error >= 3){
            		   error = 0;
            		   a = 1;
            	   }
               }
               bzero(aux, BUF_SIZE);
               size = 0;
        	break;
	}
    vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

static void  Envio_GPRS_Lat(gps_data_t* gps_data3){
	//Esta funcion envia los datos a thingspeak

	char message[318] = "Welcome to ESP32!";
	char aux[BUF_SIZE] = "";
	uint16_t size = 0, a = 0;
	uint8_t error = 0;
	e_ATCOM3 ATCOM3 = 0;

	while (a == 0){
	switch (ATCOM3){
    case CIPSTART:
    	//Para inciar la comunicacion con thingspeak
    	ESP_LOGW(TAG, "CIPSTART 1\r\n");
    	uart_write_bytes(UART_NUM_1,"AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",80\r\n", 43);
    	Tiempo_Espera(aux, ATCOM3+40,&size,error,t_CIPSTART);
    	if(strncmp(aux,"\r\nCONNECT OK",12) == 0 ||
    		strncmp(aux,"\r\nALREADY CONNECTED",7) == 0 ||
    		strncmp(aux,"\r\nOK",4) == 0){
            ATCOM3++;
            ESP_LOGW(TAG,"Aumentando ATCOM");
            error = 0;
        }else if(strncmp(aux,"\r\nERROR",7) == 0){
        	ESP_LOGE(TAG,"CIPSTART- LAT Dio Error");
            error++;
            if (error >= 3){
            	ATCOM3 = CPOWD3;
            }
        } else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
        	ESP_LOGE(TAG,"CIPSTART- LAT PDP DEACT");
        	error++;
        	pdp_deact = 1;
        	ATCOM3 = CPOWD3;
        }
        bzero(aux, BUF_SIZE);
        size = 0;
	break;
    case CIPSTART2:
    	//Para esperar el connect ok
    	ESP_LOGW(TAG, "Connect ok \r\n");
        Tiempo_Espera(aux, ATCOM3+40,&size,error,t_CIPSTART);
        if(strncmp(aux,"\r\nCONNECT OK",12) == 0 ||
        	strncmp(aux,"\r\nALREADY CONNECTED",7) == 0){
        	ATCOM3++;
        	ESP_LOGW(TAG,"Aumentando ATCOM");
        	error = 0;
        	vTaskDelay(2000 / portTICK_PERIOD_MS);
        }else if(strncmp(aux,"\r\nCMS ERROR:",12) == 0 || strncmp(aux,"\r\nERROR",7) == 0){
        	ESP_LOGE(TAG,"CIPSTART2- LAT Dio Error");
        	error++;
        	if (error >= 3){
        		ATCOM3 = CPOWD3;
        	}
        } else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
        	ESP_LOGE(TAG,"CIPSTART2- LAT PDP DEACT");
        	error++;
        	pdp_deact = 1;
        	ATCOM3 = CPOWD3;
        }
        bzero(aux, BUF_SIZE);
        size = 0;
        break;
        case CIPSEND:
        	//Para mandar datos a thingspeak
            ESP_LOGW(TAG, "CIPSEND\r\n");
            uart_write_bytes(UART_NUM_1,"AT+CIPSEND=79\r\n", 15);
            Tiempo_Espera(aux,40,&size,error,t_CIPSEND);
            vTaskDelay(300 / portTICK_PERIOD_MS);
            if(strncmp(aux,"\r\n>",3) == 0){
            	//para la latitud y longitud con lenght de 75
            	sprintf(message,"GET https://api.thingspeak.com/update?api_key=VYU3746VFOJQ2POH&field3=%.4f\r\n", gps_data3->latitude_prom);
            	uart_write_bytes(UART_NUM_1,message,79);
                ESP_LOGW(TAG,"Latitud enviada");
                error = 0;
                ATCOM3++;
        	}else if(strncmp(aux,"\r\nCMS ERROR:",12) == 0 || strncmp(aux,"\r\nERROR",7) == 0){
        		ESP_LOGE(TAG,"CIPSEND- LAT Dio Error");
        		error++;
        		if (error >= 3){
        			ATCOM3 = CPOWD3;
        		}
        	} else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
            	ESP_LOGE(TAG,"CIPSEND- LAT PDP DEACT");
            	error++;
            	pdp_deact = 1;
            	ATCOM3 = CPOWD3;
            }
        	bzero(aux, BUF_SIZE);
        	size = 0;
        	break;
        	case CIPSEND2:
        		//Para eseprar la respuesta
        	    ESP_LOGW(TAG, "CIPSEND2\r\n");
        	    Tiempo_Espera(aux, 42,&size,error,t_CIPSEND);
        	    if(strncmp(aux,"\r\nCLOSED",8) == 0){
        	        ESP_LOGW(TAG,"Socket cerrado");
        	        a = 1;
                }else if(strncmp(aux,"\r\nCMS ERROR:",12) == 0 || strncmp(aux,"\r\nERROR",7) == 0){
                    ESP_LOGE(TAG,"CIPSEND2- LAT Dio error");
                    error++;
                    if (error >= 3){
                    	ATCOM3 = CPOWD3;
                    }
                } else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
                	ESP_LOGE(TAG,"CIPSEND2- LAT PDP DEACT");
                	error++;
                	pdp_deact = 1;
                	ATCOM3 = CPOWD3;
                }
                bzero(aux, BUF_SIZE);
                size = 0;
            break;
        	case CPOWD3:
        		//Para apagar el sim800l
               ESP_LOGW(TAG, "Apagar \r\n");
               uart_write_bytes(UART_NUM_1,"AT+CPOWD=1\r\n", 12);
               Tiempo_Espera(aux, ATCOM3+30,&size,error,t_CPOWD);
               if(strncmp(aux,"\r\nNORMAL POWER DOWN",19) == 0){
            	   ESP_LOGW(TAG,"Se apago el modulo SIM800L");
            	   error = 0;
            	   a = 1;
               } else {
            	   ESP_LOGE(TAG,"CPOWD3- Dio Error");
            	   error++;
            	   if (error >= 3){
            		   error = 0;
            		   a = 1;
            	   }
               }
               bzero(aux, BUF_SIZE);
               size = 0;
        	break;
	}
    vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}


static void  Envio_GPRS_Lon(gps_data_t* gps_data3){
	//Esta funcion envia los datos a thingspeak

	char message[318] = "Welcome to ESP32!";
	char aux[BUF_SIZE] = "";
	uint16_t size = 0, a = 0;
	uint8_t error = 0;
	e_ATCOM3 ATCOM3 = 0;

	while (a == 0){
	switch (ATCOM3){
    case CIPSTART:
    	//Para inciar la comunicacion con thingspeak
    	ESP_LOGW(TAG, "CIPSTART 1\r\n");
    	uart_write_bytes(UART_NUM_1,"AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",80\r\n", 43);
    	Tiempo_Espera(aux, ATCOM3+40,&size,error,t_CIPSTART);
    	if(strncmp(aux,"\r\nCONNECT OK",12) == 0 ||
    		strncmp(aux,"\r\nALREADY CONNECTED",7) == 0 ||
    		strncmp(aux,"\r\nOK",4) == 0){
            ATCOM3++;
            ESP_LOGW(TAG,"Aumentando ATCOM");
            error = 0;
        }else if(strncmp(aux,"\r\nERROR",7) == 0){
        	ESP_LOGE(TAG,"CIPSTART- LON Dio Error");
            error++;
            if (error >= 3){
            	ATCOM3 = CPOWD3;
            }
        } else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
        	ESP_LOGE(TAG,"CIPSTART- LON PDP DEACT");
        	error++;
        	pdp_deact = 1;
        	ATCOM3 = CPOWD3;
        }
        bzero(aux, BUF_SIZE);
        size = 0;
	break;
    case CIPSTART2:
    	//Para esperar el connect ok
    	ESP_LOGW(TAG, "Connect ok \r\n");
        Tiempo_Espera(aux, ATCOM3+40,&size,error,t_CIPSTART);
        if(strncmp(aux,"\r\nCONNECT OK",12) == 0 ||
        	strncmp(aux,"\r\nALREADY CONNECTED",7) == 0){
        	ATCOM3++;
        	ESP_LOGW(TAG,"Aumentando ATCOM");
        	error = 0;
        	vTaskDelay(2000 / portTICK_PERIOD_MS);
        }else if(strncmp(aux,"\r\nCMS ERROR:",12) == 0 || strncmp(aux,"\r\nERROR",7) == 0){
        	ESP_LOGE(TAG,"CIPSTART2- LON Dio Error");
        	error++;
        	if (error >= 3){
        		ATCOM3 = CPOWD3;
        	}
        } else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
        	ESP_LOGE(TAG,"CIPSTART2- LON PDP DEACT");
        	error++;
        	pdp_deact = 1;
        	ATCOM3 = CPOWD3;
        }
        bzero(aux, BUF_SIZE);
        size = 0;
        break;
        case CIPSEND:
        	//Para mandar datos a thingspeak
            ESP_LOGW(TAG, "CIPSEND\r\n");
            uart_write_bytes(UART_NUM_1,"AT+CIPSEND=79\r\n", 15);
            Tiempo_Espera(aux,40,&size,error,t_CIPSEND);
            vTaskDelay(300 / portTICK_PERIOD_MS);
            if(strncmp(aux,"\r\n>",3) == 0){
            	//para la latitud y longitud con lenght de 75
            	sprintf(message,"GET https://api.thingspeak.com/update?api_key=VYU3746VFOJQ2POH&field4=%.4f\r\n", gps_data3->longitude_prom);
            	uart_write_bytes(UART_NUM_1,message,79);
                ESP_LOGW(TAG,"Longitud enviada");
                error = 0;
                ATCOM3++;
        	}else if(strncmp(aux,"\r\nCMS ERROR:",12) == 0 || strncmp(aux,"\r\nERROR",7) == 0){
        		ESP_LOGE(TAG,"CIPSEND- LON Dio Error");
        		error++;
        		if (error >= 3){
        			ATCOM3 = CPOWD3;
        		}
        	} else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
            	ESP_LOGE(TAG,"CIPSEND- LON PDP DEACT");
            	error++;
            	pdp_deact = 1;
            	ATCOM3 = CPOWD3;
            }
        	bzero(aux, BUF_SIZE);
        	size = 0;
        	break;
        	case CIPSEND2:
        		//Para eseprar la respuesta
        	    ESP_LOGW(TAG, "CIPSEND2\r\n");
        	    Tiempo_Espera(aux, 42,&size,error,t_CIPSEND);
        	    if(strncmp(aux,"\r\nCLOSED",8) == 0){
        	        ESP_LOGW(TAG,"Socket cerrado");
        	        a = 1;
                }else if(strncmp(aux,"\r\nCMS ERROR:",12) == 0 || strncmp(aux,"\r\nERROR",7) == 0){
                    ESP_LOGE(TAG,"CIPSEND2- LON Dio error");
                    error++;
                    if (error >= 3){
                    	ATCOM3 = CPOWD3;
                    }
                } else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
                	ESP_LOGE(TAG,"CIPSEND2- LON PDP DEACT");
                	error++;
                	pdp_deact = 1;
                	ATCOM3 = CPOWD3;
                }
                bzero(aux, BUF_SIZE);
                size = 0;
            break;
        	case CPOWD3:
        		//Para apagar el sim800l
               ESP_LOGW(TAG, "Apagar \r\n");
               uart_write_bytes(UART_NUM_1,"AT+CPOWD=1\r\n", 12);
               Tiempo_Espera(aux, ATCOM3+30,&size,error,t_CPOWD);
               if(strncmp(aux,"\r\nNORMAL POWER DOWN",19) == 0){
            	   ESP_LOGW(TAG,"Se apago el modulo SIM800L");
            	   error = 0;
            	   a = 1;
               } else {
            	   ESP_LOGE(TAG,"CPOWD3- Dio Error");
            	   error++;
            	   if (error >= 3){
            		   error = 0;
            		   a = 1;
            	   }
               }
               bzero(aux, BUF_SIZE);
               size = 0;
        	break;
	}
    vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

static void  Envio_GPRS_Puerta(){
	//Esta funcion envia los datos a thingspeak

	char message[318] = "Welcome to ESP32!";
	char aux[BUF_SIZE] = "";
	uint16_t size = 0, a = 0;
	uint8_t error = 0;
	e_ATCOM3 ATCOM3 = 0;

	while (a == 0){
	switch (ATCOM3){
    case CIPSTART:
    	//Para inciar la comunicacion con thingspeak
    	ESP_LOGW(TAG, "CIPSTART 1\r\n");
    	uart_write_bytes(UART_NUM_1,"AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",80\r\n", 43);
    	Tiempo_Espera(aux, ATCOM3+40,&size,error,t_CIPSTART);
    	if(strncmp(aux,"\r\nCONNECT OK",12) == 0 ||
    		strncmp(aux,"\r\nALREADY CONNECTED",7) == 0 ||
    		strncmp(aux,"\r\nOK",4) == 0){
            ATCOM3++;
            ESP_LOGW(TAG,"Aumentando ATCOM");
            error = 0;
        }else if(strncmp(aux,"\r\nERROR",7) == 0){
        	ESP_LOGE(TAG,"CIPSTART- PUERTA Dio Error");
            error++;
            if (error >= 3){
            	ATCOM3 = CPOWD3;
            }
        } else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
        	ESP_LOGE(TAG,"CIPSTART- PUERTA PDP DEACT");
        	error++;
        	pdp_deact = 1;
        	ATCOM3 = CPOWD3;
        }
        bzero(aux, BUF_SIZE);
        size = 0;
	break;
    case CIPSTART2:
    	//Para esperar el connect ok
    	ESP_LOGW(TAG, "Connect ok \r\n");
        Tiempo_Espera(aux, ATCOM3+40,&size,error,t_CIPSTART);
        if(strncmp(aux,"\r\nCONNECT OK",12) == 0 ||
        	strncmp(aux,"\r\nALREADY CONNECTED",7) == 0){
        	ATCOM3++;
        	ESP_LOGW(TAG,"Aumentando ATCOM");
        	error = 0;
        	vTaskDelay(2000 / portTICK_PERIOD_MS);
        }else if(strncmp(aux,"\r\nCMS ERROR:",12) == 0 || strncmp(aux,"\r\nERROR",7) == 0){
        	ESP_LOGE(TAG,"CIPSTART2- PUERTA Dio Error");
        	error++;
        	if (error >= 3){
        		ATCOM3 = CPOWD3;
        	}
        } else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
        	ESP_LOGE(TAG,"CIPSTART2- PUERTA PDP DEACT");
        	error++;
        	pdp_deact = 1;
        	ATCOM3 = CPOWD3;
        }
        bzero(aux, BUF_SIZE);
        size = 0;
        break;
        case CIPSEND:
        	//Para mandar datos a thingspeak
            ESP_LOGW(TAG, "CIPSEND\r\n");
            uart_write_bytes(UART_NUM_1,"AT+CIPSEND=75\r\n", 15);
            Tiempo_Espera(aux,40,&size,error,t_CIPSEND);
            vTaskDelay(300 / portTICK_PERIOD_MS);
            if(strncmp(aux,"\r\n>",3) == 0){
            	//para la latitud y longitud con lenght de 75
            	sprintf(message,"GET https://api.thingspeak.com/update?api_key=VYU3746VFOJQ2POH&field5=100\r\n");
            	uart_write_bytes(UART_NUM_1,message,75);
                ESP_LOGW(TAG,"Longitud enviada");
                error = 0;
                ATCOM3++;
        	}else if(strncmp(aux,"\r\nCMS ERROR:",12) == 0 || strncmp(aux,"\r\nERROR",7) == 0){
        		ESP_LOGE(TAG,"CIPSEND- LON Dio Error");
        		error++;
        		if (error >= 3){
        			ATCOM3 = CPOWD3;
        		}
        	} else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
            	ESP_LOGE(TAG,"CIPSEND- LON PDP DEACT");
            	error++;
            	pdp_deact = 1;
            	ATCOM3 = CPOWD3;
            }
        	bzero(aux, BUF_SIZE);
        	size = 0;
        	break;
        	case CIPSEND2:
        		//Para eseprar la respuesta
        	    ESP_LOGW(TAG, "CIPSEND2\r\n");
        	    Tiempo_Espera(aux, 42,&size,error,t_CIPSEND);
        	    if(strncmp(aux,"\r\nCLOSED",8) == 0){
        	        ESP_LOGW(TAG,"Socket cerrado");
        	        a = 1;
                }else if(strncmp(aux,"\r\nCMS ERROR:",12) == 0 || strncmp(aux,"\r\nERROR",7) == 0){
                    ESP_LOGE(TAG,"CIPSEND2- LON Dio error");
                    error++;
                    if (error >= 3){
                    	ATCOM3 = CPOWD3;
                    }
                } else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
                	ESP_LOGE(TAG,"CIPSEND2- LON PDP DEACT");
                	error++;
                	pdp_deact = 1;
                	ATCOM3 = CPOWD3;
                }
                bzero(aux, BUF_SIZE);
                size = 0;
            break;
        	case CPOWD3:
        		//Para apagar el sim800l
               ESP_LOGW(TAG, "Apagar \r\n");
               uart_write_bytes(UART_NUM_1,"AT+CPOWD=1\r\n", 12);
               Tiempo_Espera(aux, ATCOM3+30,&size,error,t_CPOWD);
               if(strncmp(aux,"\r\nNORMAL POWER DOWN",19) == 0){
            	   ESP_LOGW(TAG,"Se apago el modulo SIM800L");
            	   error = 0;
            	   a = 1;
               } else {
            	   ESP_LOGE(TAG,"CPOWD3- Dio Error");
            	   error++;
            	   if (error >= 3){
            		   error = 0;
            		   a = 1;
            	   }
               }
               bzero(aux, BUF_SIZE);
               size = 0;
        	break;
	}
    vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

static void  Enviar_GPRS(gps_data_t* gps_data2, AM2301_data_t* Thum2 ){

	// Esta funcion envia los datos correspondientes por GPRS a thingspeak

	char message[318] = "Welcome to ESP32!";
//	char aux[BUF_SIZE] = "";


	//El tiempo de espera entre datos de thingspeak es de 15 segundos

    if (Thum2->error_temp == 0){
    	switch(limite_b){
    	case 0:
    		Envio_GPRS_temp(Thum2);
    		ESP_LOGW("TAG", "Envio temperatura por GPRS");
    	    vTaskDelay(10000 / portTICK_PERIOD_MS);
    		Envio_GPRS_hum(Thum2);
    		ESP_LOGW("TAG", "Envio humedad por GPRS");
    		vTaskDelay(10000 / portTICK_PERIOD_MS);
    	break;
    	case 1:
    		Envio_GPRS_temp(Thum2);
    		ESP_LOGW("TAG", "Envio temperatura por GPRS");
    	    vTaskDelay(10000 / portTICK_PERIOD_MS);
    		Envio_GPRS_hum(Thum2);
    		ESP_LOGW("TAG", "Envio humedad por GPRS");
    		//Si enviara datos gps necesito esperar
    		if (gps_data2->error_gps == 0){
    			vTaskDelay(10000 / portTICK_PERIOD_MS);
    		}
    		if (limite_a == 0){
			limite_b = 0;
    		}
    	break;
    	}
    } else {
    	Thum.error_temp = 0;
    	sprintf(message,"2-No se logro medir la temepratura. Revisar las conexiones.");
    }

    switch (puerta_b){
    case P_cerrada:
    // Verifico si no hubo error al conectarse a al modulo GPS
        if (gps_data2->error_gps == 0){
        	//Verifico si se conecto a la red GPS viendo si devolvio que es el a;o 2020
        	if (gps_data2->year == 20){
        		Envio_GPRS_Lat(gps_data2);
        		ESP_LOGW("TAG", "Envio latitud por GPRS");
        	    vTaskDelay(10000 / portTICK_PERIOD_MS);
        	    Envio_GPRS_Lon(gps_data2);
        	    ESP_LOGW("TAG", "Envio longitud por GPRS");
        	} else{
        		 ESP_LOGI(TAG, "2-NO se conecto a la red GPS");
        	}
        } else{
  			gps_data.error_gps = 0;;
    		ESP_LOGI(TAG, "2-No se comunico con el modulo GPS");
        }
    break;
    case P_abierta:
    	// Verifico si no hubo error al conectarse a al modulo GPS
    	if (gps_data2->error_gps == 0){
    	//Verifico si se conecto a la red GPS viendo si devolvio que es el a;o 2020
    		if (gps_data2->year == 20){
    			Envio_GPRS_Puerta();
    			ESP_LOGW("TAG", "Envio estado de puerta por GPRS");
    			vTaskDelay(10000 / portTICK_PERIOD_MS);
        		Envio_GPRS_Lat(gps_data2);
        		ESP_LOGW("TAG", "Envio latitud por GPRS");
        	    vTaskDelay(10000 / portTICK_PERIOD_MS);
        	    Envio_GPRS_Lon(gps_data2);
        	    ESP_LOGW("TAG", "Envio longitud por GPRS");
    			if (puerta_a == 0){
    				puerta_b = 0;
    				puerta_c = 0;
    			}
    	    } else{
    	    	Envio_GPRS_Puerta();
    	    	ESP_LOGI(TAG, "Puerta abierta, sin conexion gps");
    			if (puerta_a == 0){
    				puerta_b = 0;
    				puerta_c = 0;
    			}
    	    }
    	} else{
			gps_data.error_gps = 0;
			Envio_GPRS_Puerta();
			ESP_LOGI(TAG, "2- Puerta abierta, no hay gps");
			if (puerta_a == 0){
				puerta_b = 0;
				puerta_c = 0;
			}
    	}
    break;
    }
}

static void  Enviar_Mensaje(gps_data_t* gps_data2, AM2301_data_t* Thum2)
{
	//Esta funcion se encarga de enviar el mensaje de texto adecuado
	char message[318] = "Welcome to ESP32!";
	char aux[BUF_SIZE] = "";
	uint16_t size = 0;
	e_TEspera T_Espera;
	uint8_t error = 0;

    if (Thum2->error_temp == 0){
    	switch(limite_b){
    	case 0:
    		sprintf(message,"La humedad es: %.1f %% y la temperatura es: %.1f C",Thum2->Prom_hum[Thum2->pos_temp-1],Thum2->Prom_temp[Thum2->pos_temp-1]);
    		Envio_mensaje(message,48);
    		ESP_LOGI(TAG, "Send send message [%s] ok", message);
    	break;
    	case 1:
    		sprintf(message,"La temperatura se salio de los limites. La humedad es: %.1f  %% y la temperatura es: %.1f C",Thum2->Prom_hum[Thum2->pos_temp-1],Thum2->Prom_temp[Thum2->pos_temp-1]);
    		Envio_mensaje(message,89);
    		ESP_LOGI(TAG, "Send send message [%s] ok", message);
    		if (limite_a == 0){
			limite_b = 0;
    		}
    	break;
    	}
    } else {
    	Thum.error_temp = 0;
    	sprintf(message,"No se logro medir la temepratura. Revisar las conexiones.");
    	Envio_mensaje(message,57);
    	ESP_LOGI(TAG, "Send send message [%s] ok", message);
    }

    switch (puerta_b){
    case P_cerrada:
    // Verifico si no hubo error al conectarse a al modulo GPS
        if (gps_data2->error_gps == 0){
        	//Verifico si se conecto a la red GPS viendo si devolvio que es el a;o 2020
        	if (gps_data2->year == 20){
			    sprintf(message,"La latitud es: %.4f %s y la longitud es: %.4f %s",gps_data2->latitude_prom,gps_data2->latitude_direct,gps_data2->longitude_prom,gps_data2->longitude_direct);
			    Envio_mensaje(message,87);
			    ESP_LOGI(TAG, "Send send message [%s] ok", message);

        		sprintf(message,"Las medidas se realizaron el %d de %s de 20%d a las %d horas con %d minutos y %d segundos",gps_data2->day,gps_data2->mes,gps_data2->year,gps_data2->hour,gps_data2->minute,gps_data2->second);
        		Envio_mensaje(message,112);
        		ESP_LOGI(TAG, "Send send message [%s] ok", message);
        	} else{

        		 sprintf(message,"No se logro conectar a la red GPS.");
        		 Envio_mensaje(message,34);
        		 ESP_LOGI(TAG, "Send send message [%s] ok", message);
        	}
        } else{
  			gps_data.error_gps = 0;
    		sprintf(message,"No se logro conectar con el modulo GPS.");
    		Envio_mensaje(message,39);
    		ESP_LOGI(TAG, "Send send message [%s] ok", message);
        }
    break;
    case P_abierta:
    	// Verifico si no hubo error al conectarse a al modulo GPS
    	if (gps_data2->error_gps == 0){
    	//Verifico si se conecto a la red GPS viendo si devolvio que es el a;o 2020
    		if (gps_data2->year == 20){

    			 sprintf(message,"La puerta fue abierta. La latitud es: %.4f %s y la longitud es: %.4f %s",gps_data2->latitude_prom,gps_data2->latitude_direct,gps_data2->longitude_prom,gps_data2->longitude_direct);
    			 Envio_mensaje(message,87);
    			 ESP_LOGI(TAG, "Send send message [%s] ok", message);
    			if (puerta_a == 0){
    				puerta_b = 0;
    				puerta_c = 0;
    			}

    	         sprintf(message,"Las medidas se realizaron el %d de %s de 20%d a las %d horas con %d minutos y %d segundos",gps_data2->day,gps_data2->mes,gps_data2->year,gps_data2->hour,gps_data2->minute,gps_data2->second);
    	         Envio_mensaje(message,112);
    	         ESP_LOGI(TAG, "Send send message [%s] ok", message);
    	    } else{

    	    	 sprintf(message,"La puerta se abrio, pero no se logro conectar a la red GPS.");
    	    	 Envio_mensaje(message,59);
    	    	 ESP_LOGI(TAG, "Send send message [%s] ok", message);
    			if (puerta_a == 0){
    				puerta_b = 0;
    				puerta_c = 0;
    			}
    	    }
    	} else{
			gps_data.error_gps = 0;
			sprintf(message,"La puerta se abrio, pero no se logro conectar con el modulo GPS.");
			Envio_mensaje(message,64);
			ESP_LOGI(TAG, "Send send message [%s] ok", message);
			if (puerta_a == 0){
				puerta_b = 0;
				puerta_c = 0;
			}
    	}
    break;
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
	e_ATCOM ATCOM = CMGF;
	e_ATCOM2 ATCOM2 = CFUN;
	gps_data_t gps_data2;
	AM2301_data_t Thum2;
	message_data_t message_data;
	uint8_t prender_apagar = 0;


	printf("Entre en mandar mensaje \r\n");
	for(;;){

		xEventGroupSync(event_group,BEGIN_TASK1,0xC0,portMAX_DELAY);
	   	xEventGroupClearBits(event_group, SYNC_BIT_TASK1);
	    xEventGroupClearBits(event_group, SYNC_BIT_TASK2);



	    /* Apagar el modulo por si esta prendido al entrar en la tarea */
    	if  (prender_apagar == 0){
    		ESP_LOGW(TAG, "Apagare y prendere\r\n");
    	    uart_write_bytes(UART_NUM_1,"AT+CPOWD=1\r\n", 12);
    	    vTaskDelay(2000 / portTICK_PERIOD_MS);
    	    Prender_SIM800l();
    	}

	    //Recibo la informacion de la temperatura
	    if(xQueueReceive(xQueue_temp,&Thum2,(portTickType) 4000 / portTICK_PERIOD_MS)) {
	    	sprintf(message_data.Humedad, "%f",Thum2.Prom_hum[Thum2.pos_temp-1]);
	    	sprintf(message_data.Temperatura, "%f",Thum2.Prom_temp[Thum2.pos_temp-1]);
	    } else {
	    	Thum2.error_temp = 1;
	    }

    	//Recibo la informacion del gps
	    if(xQueueReceive(xQueue_gps,&gps_data2,(portTickType) 1500 / portTICK_PERIOD_MS)) {
	    	sprintf(message_data.Latitude, "%f",gps_data2.latitude_prom);
	    	sprintf(message_data.Longitude, "%f",gps_data2.longitude_prom);
	    	sprintf(message_data.Latitude_dir, "%s",gps_data2.latitude_direct);
	    	sprintf(message_data.Longitude_dir, "%s",gps_data2.longitude_direct);
	    } else {
	    	gps_data2.error_gps = 1;
	    }
	    ESP_LOGE(TAG, "Error GPS es %d\r\n",gps_data2.error_gps);

    	//Verifico si se supero el limite de temperatura o se abrio la puerta para guardar la informacion
    	//en la memoria flash
    	if (limite_b == 1 || puerta_b == 1){
    		ESP_LOGW(TAG, "Guardare en la memoria flash\r\n");
        	set_form_flash_init(&message_data);
        	}

	ESP_LOGW(TAG, "Verificara el estado de la red para poder enviar mensajes\r\n");

	ATCOM = Configurar_GSM(ATCOM);


    //Si la red esta conectada entonces entonces se envia lo mensajes y luego se configura el GPRS
    if (ATCOM != CPOWD){
    	Enviar_Mensaje(&gps_data2,&Thum2);
    	//Si es la primera vuelta configuro el GPRS
    	if  (prender_apagar == 0){
        	ATCOM2 = Configurar_GPRS(ATCOM2);
        	prender_apagar = 1;
    	}
    }

    ESP_LOGE(TAG, "Error GPS es %d\r\n",gps_data2.error_gps);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    //Si se logro configurar correctamente y activar el GPRS se enviando los datos
    if (ATCOM2 != CPOWD2 && pdp_deact == 0 ){
    	Enviar_GPRS(&gps_data2,&Thum2);
    }

	//Verifico si no se apago o si dio error de pdp_deact
	//para que en la siguiente vuelta apague y prenda el sim800
    if( pdp_deact == 1 || ATCOM == CPOWD || ATCOM2 == CPOWD2 ){
    	prender_apagar = 0;
    	pdp_deact = 0;
    }
    //Reinicio los valores de ATCOM para la siguiente ronda
	ATCOM = CMGF;
	ATCOM2 = CFUN;
    vTaskDelay(100 / portTICK_PERIOD_MS);
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

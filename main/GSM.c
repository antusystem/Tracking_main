/*
 * GSM.c
 *
 *  Created on: Jul 22, 2020
 *      Author: Antunes
 *
 * GSM: se encarga de recibir los datos GPS y de temperatura por cola
 * para luego enviarlos por mensaje de texto y por GPRS a thingspeak
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

/*Definir los pines conectados al SIM800l*/
#define SIM800l_PWR_KEY (4)
#define SIM800l_PWR (23)
#define SIM800l_RST (5)

/* Envios por GSM */

#define Phone_Number "+584242428865"
//Campos de Thingspeak, APIKEY de Thingspeak
#define Campo_temp 1
#define Campo_hum 2
#define Campo_lat 3
#define Campo_lon 4
#define Campo_puerta 5
#define API_KEY "VYU3746VFOJQ2POH"

/* variables externas*/

extern uint8_t limite_a;
extern uint8_t limite_b;
extern uint8_t puerta_a, puerta_c, puerta_d, puerta_e;
extern e_Puerta puerta_b;

//Variable para saber si dio error PDP_deact

uint8_t pdp_deact = 0;

//Pines del uart conectados al SIM800l
#define MEN_TXD  (GPIO_NUM_27)
#define MEN_RXD  (GPIO_NUM_26)
#define MEN_RTS  (UART_PIN_NO_CHANGE)
#define MEN_CTS  (UART_PIN_NO_CHANGE)

//Para el grupo de eventos

extern EventGroupHandle_t event_group;
extern const int BEGIN_TASK1;
extern const int SYNC_BIT_TASK1;
extern const int SYNC_BIT_TASK2;

// Para las colas
extern QueueHandle_t xQueue_temp;
extern QueueHandle_t xQueue_gps;
extern QueueHandle_t Datos_uart1;

static const char *TAG = "Datos GSM";

void  Tiempo_Espera(char* aux, uint8_t estado, uint16_t* tamano, uint8_t* error, portTickType tiempo){
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

void  Prender_SIM800l(){
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

void  Envio_mensaje(char* mensaje, uint8_t tamano){
	//Esta funcion enviara el mensaje que se le pase
	char aux[BUF_SIZE] = "32!";
	uint16_t size = 0;
	uint8_t error = 0, jail = 0,lenght = 0;
	const char* finalSMSComand = "\x1A";

	//Se manda el comando AT para comenzar el envio del mensaje
	ESP_LOGW("Mensaje","Mandare el mensaje");
	bzero(aux,BUF_SIZE);
	sprintf(aux,"AT+CMGS=\"%s\"\r\n",Phone_Number);
	lenght = strlen(aux);
	uart_write_bytes(UART_NUM_1,aux, lenght);

	//Se espera la respuesta y luego se compara a ver si es la esperada
	//La respuesta es "inmediata" asi que no deberia hacer falta ponerlo en un ciclo
	Tiempo_Espera(aux, 20,&size,&error, t_CMGS);
    vTaskDelay(300 / portTICK_PERIOD_MS);
    //Se compara si es la respuesta esperada
	if(strncmp(aux,"\r\n>",3) == 0){
		uart_write_bytes(UART_NUM_1,mensaje,tamano);
		uart_write_bytes(UART_NUM_1,(const char*)finalSMSComand, 2);
		error = 0;
	}else if(strncmp(aux,"\r\nCMS ERROR:",12) == 0 || strncmp(aux,"\r\nERROR",7) == 0){
		ESP_LOGE("Mensaje","No se pudo mandar el mensaje");
		error++;
	}else if (error >= 1){
		jail = 1;
	}
    ESP_LOGW(TAG, "Mensaje2 \r\n");
    vTaskDelay(500 / portTICK_PERIOD_MS);
    //Aqui espero la respuesta al envio de mensaje para poder terminar la tarea y no molestar
    // en el siguiente mensaje
    while (jail == 0){
    	Tiempo_Espera(aux, 21,&size,&error,t_CMGS);
    	if(strncmp(aux,"\r\n+CMGS:",8) == 0){
    		jail = 1;
    	}else if(strncmp(aux,"\r\nCMS ERROR:",12) == 0 || strncmp(aux,"\r\nERROR",7) == 0){
    		ESP_LOGE(TAG,"21- Dio Error");
    		error++;
    		if (error >= 1){
    			ESP_LOGE(TAG,"21- No se mando el mensaje Error");
    			jail = 1;
    			break;
    		}
    	}else {
    		error++;
    		if (error >= 2){
    			jail = 1;
    		}
    	}
    vTaskDelay(700 / portTICK_PERIOD_MS);
    }

	vTaskDelay(1000 / portTICK_PERIOD_MS);
}

e_ATCOM  Configurar_GSM(e_ATCOM ATCOM){
	//Esta funcion de configurar el sim800 para enviar mensajes de texto
	ATCOM = CMGF;
	char aux[BUF_SIZE] = "!";
	uint16_t size = 0;
	uint8_t  config = 0;
	uint8_t flags_errores = 0, flags3_errores = 0;
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
            } else if (flags_errores >= 2){
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
           } else if (flags_errores >= 2){
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

e_ATCOM2  Configurar_GPRS(e_ATCOM2 ATCOM){
	//Esta funcion de configurar el sim800 para enviar datos GPRS
	uint8_t a = 0, error = 0;
	ATCOM = CFUN;
	char aux[BUF_SIZE] = "32!";
	uint16_t size = 0;


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
            Tiempo_Espera(aux, ATCOM+30,&size,&error, t_CFUN);
            if(strncmp(aux,"\r\nOK",4) == 0){
            	ATCOM++;
            	ESP_LOGW(TAG,"Aumentando ATCOM");
            	error = 0;
            }else if(strncmp(aux,"\r\nCME ERROR:",12) == 0 || strncmp(aux,"\r\nERROR",7) == 0){
            	ESP_LOGE(TAG,"CFUN- Dio Error");
            	error++;
            	if (error >= 3){
            		ATCOM = CPOWD2;
            	}
            } else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
            	ESP_LOGE(TAG,"CFUN- PDP DEACT");
            	error++;
            	pdp_deact = 1;
            	ATCOM = CPOWD2;
            } else if (error >= 2){
            	ATCOM = CPOWD;
            }
            bzero(aux, BUF_SIZE);
            size = 0;
    	break;
    	case CSTT:
            //Para conectarse a la red de Movistar
            ESP_LOGW(TAG,"Mando CSTT");
            uart_write_bytes(UART_NUM_1,"AT+CSTT=\"internet.movistar.ve\",\"\",\"\"\r\n", 39);
            ESP_LOGW(TAG, "Conectandose a movistar \r\n");
            Tiempo_Espera(aux,ATCOM+30,&size,&error,t_CSST);
            if(strncmp(aux,"\r\nOK",4) == 0){
            	ATCOM++;
            	ESP_LOGW(TAG,"Aumentando ATCOM");
            	error = 0;
            	vTaskDelay(10000 / portTICK_PERIOD_MS);
            } else if(strncmp(aux,"\r\nERROR",7) == 0 ){
            	ESP_LOGE(TAG,"CSTT- Dio error");
            	error++;
            	if (error >= 3){
            		ATCOM = CPOWD2;
            	}
            } else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
            	ESP_LOGE(TAG,"CMGF- PDP DEACT");
            	error++;
            	pdp_deact = 1;
            	ATCOM = CPOWD2;
            } else if (error >= 2){
            	ATCOM = CPOWD;
            }
            bzero(aux, BUF_SIZE);
            size = 0;
    	break;
    	case CIICR:
            // Para activar la conexion inalambrica por GPRS
    	    ESP_LOGW(TAG, "Mando Ciirc\r\n");
    		uart_write_bytes(UART_NUM_1,"AT+CIICR\r\n", 10);
    	    ESP_LOGW(TAG, "Ciirc activando \r\n");
    	    Tiempo_Espera(aux, ATCOM+30,&size,&error,t_CIICR);
            if(strncmp(aux,"\r\nOK",4) == 0){
            	ATCOM++;
            	ESP_LOGW(TAG,"Aumentando ATCOM");
            	error = 0;
            }else if(strncmp(aux,"\r\nERROR",7) == 0 ){
            	ESP_LOGE(TAG,"CIICR- Dio error");
            	error++;
            	if (error >= 2){
            		ATCOM = CPOWD2;
            	}
            }else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
            	ESP_LOGE(TAG,"CIICR- PDP DEACT");
            	error++;
            	pdp_deact = 1;
            	ATCOM = CPOWD2;
            } else if (error >= 1){
            	ATCOM = CPOWD;
            }
            bzero(aux, BUF_SIZE);
            size = 0;
    	break;
    	case CGREG:
    		//Verificando que este conectado al GPRS
    		ESP_LOGW(TAG, "Mando CGREG\r\n");
    		uart_write_bytes(UART_NUM_1,"AT+CGREG?\r\n", 11);
    		Tiempo_Espera(aux, ATCOM+30,&size,&error,t_CGREG);
            if(strncmp(aux,"\r\n+CGREG: 0,1",13) == 0){
            	ATCOM++;
            	ESP_LOGW(TAG,"Aumentando ATCOM");
            	error = 0;
            }else if(strncmp(aux,"\r\nCME ERROR:",12) == 0 || strncmp(aux,"\r\nERROR",7) == 0){
            	ESP_LOGE(TAG,"CGREG- Dio Error");
            	error++;
            	if (error >= 3){
            		ATCOM = CPOWD2;
            	}
            } else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
            	ESP_LOGE(TAG,"CGREG- PDP DEACT");
            	error++;
            	pdp_deact = 1;
            	ATCOM = CPOWD2;
            } else if (error >= 2){
            	ATCOM = CPOWD;
            }
            bzero(aux, BUF_SIZE);
            size = 0;
    	break;
    	case CIFSR:
		       // Para pedir la ip asignada
		    uart_write_bytes(UART_NUM_1,"AT+CIFSR\r\n", 10);
		    ESP_LOGW(TAG, "Pidiendo IP \r\n");
		    Tiempo_Espera(aux, ATCOM+30,&size,&error,t_CIFSR);
		    //BUsco si lo que llego al uart tiene un . para ver si respondio la ip
		    char* ip = strstr(aux,".");
		    if (ip == NULL || strncmp(aux,"\r\nERROR",7) == 0){
		    	ESP_LOGE(TAG,"CIFSR- Dio Error");
		    	error++;
		    	if (error >= 3){
		    		ATCOM = CPOWD2;
		    	}
            } else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
            	ESP_LOGE(TAG,"COPWD2- PDP DEACT");
            	error++;
            	pdp_deact = 1;
            	ATCOM = CPOWD2;
            } else if (error >= 2){
            	ATCOM = CPOWD;
            } else {
            	a = 1;
            	error = 0;
            }
            bzero(aux, BUF_SIZE);
            size = 0;
         break;
    	case CPOWD2:
    		//Para apagar el sim800l
            ESP_LOGW(TAG, "Apagar \r\n");
            uart_write_bytes(UART_NUM_1,"AT+CPOWD=1\r\n", 12);
            Tiempo_Espera(aux, ATCOM+30,&size,&error,t_CPOWD);
            if(strncmp(aux,"\r\nNORMAL POWER DOWN",19) == 0){
            	ESP_LOGW(TAG,"Se apago el modulo SIM800L");
            	error = 0;
            }  else if(strncmp(aux,"\r\n+PDP: DEACT",7) == 0){
            	ESP_LOGE(TAG,"CPOWD2- PDP DEACT");
            	error++;
            	if (error >= 3){
            		error = 0;
             		a = 1;
            	}
            } else {
            	ESP_LOGE(TAG,"CPOWD2- Dio Error");
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
	return ATCOM;
}

//sprintf(message,"GET https://api.thingspeak.com/update?api_key=VYU3746VFOJQ2POH&field4=%.6f\r\n", gps_data3->longitude_prom);

void  Envio_GPRS(uint8_t gps_temp, uint8_t campo,float dato){
	//Esta funcion envia los datos a thingspeak

	char message[318] = "Welcome to ESP32!";
	char aux[BUF_SIZE] = "";
	uint16_t size = 0, a = 0, lenght = 0, lenght2 = 0;
	uint8_t error = 0;
	e_ATCOM3 ATCOM3 = 0;

	while (a == 0){
		switch (ATCOM3){
		case CIPSTART:
			//Para inciar la comunicacion con thingspeak
			ESP_LOGW(TAG, "CIPSTART 1\r\n");
			uart_write_bytes(UART_NUM_1,"AT+CIPSTART=\"TCP\",\"api.thingspeak.com\",80\r\n", 43);
			Tiempo_Espera(aux, ATCOM3+40,&size,&error,t_CIPSTART);
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
			} else if (error >= 1){
        	ATCOM3 = CPOWD3;
			}
			bzero(aux, BUF_SIZE);
			size = 0;
			break;
    case CIPSTART2:
    	//Para esperar el connect ok
    	ESP_LOGW(TAG, "Connect ok \r\n");
        Tiempo_Espera(aux, ATCOM3+40,&size,&error,t_CIPSTART);
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
        } else if (error >= 1){
        	ATCOM3 = CPOWD3;
        }
        bzero(aux, BUF_SIZE);
        size = 0;
        break;
        case CIPSEND:
        	//Para mandar datos a thingspeak
            ESP_LOGW(TAG, "CIPSEND\r\n");
            //Verifico si enviare datos de temperatura/humedad o de gps
            if (gps_temp == Dato_temp){
            	sprintf(message,"GET https://api.thingspeak.com/update?api_key=%s&field%d=%.1f\r\n",API_KEY,campo,dato);
            } else if ( gps_temp == Dato_gps){
            	sprintf(message,"GET https://api.thingspeak.com/update?api_key=%s&field%d=%.6f\r\n",API_KEY,campo,dato);
            } else if (gps_temp == Dato_puerta) {
            	sprintf(message,"GET https://api.thingspeak.com/update?api_key=%s&field%d=%d\r\n",API_KEY,campo,(int) dato);
            }
            lenght = strlen(message);
            sprintf(aux,"AT+CIPSEND=%d\r\n",lenght);
            lenght2 = strlen(aux);
            uart_write_bytes(UART_NUM_1,aux,lenght2);
            bzero(aux, BUF_SIZE);
            Tiempo_Espera(aux,40,&size,&error,t_CIPSEND);
            vTaskDelay(300 / portTICK_PERIOD_MS);
            if(strncmp(aux,"\r\n>",3) == 0){
            	//para la latitud y longitud con lenght de 75
     //       	sprintf(message,"GET https://api.thingspeak.com/update?api_key=VYU3746VFOJQ2POH&field5=100\r\n");
     //       	lenght = strlen(message);
            	uart_write_bytes(UART_NUM_1,message,lenght);
                ESP_LOGW(TAG,"Dato enviado por GPRS");
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
            } else if (error >= 1){
            	ATCOM3 = CPOWD3;
            }
        	bzero(aux, BUF_SIZE);
        	size = 0;
        	break;
        	case CIPSEND2:
        		//Para eseprar la respuesta
        	    ESP_LOGW(TAG, "CIPSEND2\r\n");
        	    Tiempo_Espera(aux, 42,&size,&error,t_CIPSEND);
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
                } else if (error >= 1){
                	ATCOM3 = CPOWD3;
                }
                bzero(aux, BUF_SIZE);
                size = 0;
            break;
        	case CPOWD3:
        		//Para apagar el sim800l
               ESP_LOGW(TAG, "Apagar \r\n");
               uart_write_bytes(UART_NUM_1,"AT+CPOWD=1\r\n", 12);
               Tiempo_Espera(aux, ATCOM3+30,&size,&error,t_CPOWD);
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

void  Enviar_GPRS(gps_data_t* gps_data2, AM2301_data_t* Thum2){

	// Esta funcion envia los datos correspondientes por GPRS a thingspeak

	char message[318] = "Welcome to ESP32!";

	//El tiempo de espera entre datos de thingspeak es de 15 segundos

    if (Thum2->error_temp == 0){
    	if (Thum2->primer_ciclo == 0){
    		switch(limite_b){
    		case 0:
    		//	Envio_GPRS_temp(Thum2);
    			Envio_GPRS(Dato_temp,Campo_temp, Thum2->Prom_temp[Thum2->pos_temp-1]);
    			ESP_LOGW("TAG", "Envio temperatura por GPRS");
    			vTaskDelay(10000 / portTICK_PERIOD_MS);
    		//	Envio_GPRS_hum(Thum2);
    			Envio_GPRS(Dato_temp,Campo_hum, Thum2->Prom_hum[Thum2->pos_temp-1]);
    			ESP_LOGW("TAG", "Envio humedad por GPRS");
    			vTaskDelay(10000 / portTICK_PERIOD_MS);
    			break;
    		case 1:
    		//	Envio_GPRS_temp(Thum2);
    			Envio_GPRS(Dato_temp,Campo_temp, Thum2->Prom_temp[Thum2->pos_temp-1]);
    			ESP_LOGW("TAG", "Envio temperatura por GPRS");
    			vTaskDelay(10000 / portTICK_PERIOD_MS);
    		//	Envio_GPRS_hum(Thum2);
    			Envio_GPRS(Dato_temp,Campo_hum, Thum2->Prom_hum[Thum2->pos_temp-1]);
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
    		ESP_LOGE("TAG", "No se ha terminado el primer ciclo de la medicion de temperatura");
    	}
    } else {
    	Thum.error_temp = 0;
    	sprintf(message,"2-No se logro medir la temperatura. Revisar las conexiones.");
    }

    switch (puerta_b){
    case P_cerrada:
    // Verifico si no hubo error al conectarse a al modulo GPS
        if (gps_data2->error_gps == 0){
        	//Verifico si se conecto a la red GPS viendo si devolvio que es el a;o 2020
        	if (gps_data2->year == 20){
        	//	Envio_GPRS_Lat(gps_data2);
        		Envio_GPRS(Dato_gps,Campo_lat, gps_data2->latitude_prom);
        		ESP_LOGW("TAG", "Envio latitud por GPRS");
        	    vTaskDelay(10000 / portTICK_PERIOD_MS);
        	//    Envio_GPRS_Lon(gps_data2);
        	    Envio_GPRS(Dato_gps,Campo_lon, gps_data2->longitude_prom);
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
    			Envio_GPRS(Dato_gps,Campo_puerta, 100.0);
    			ESP_LOGW("TAG", "Envio estado de puerta por GPRS");
    			vTaskDelay(10000 / portTICK_PERIOD_MS);
        	//	Envio_GPRS_Lat(gps_data2);
    			Envio_GPRS(Dato_gps,Campo_lat, gps_data2->latitude_prom);
        		ESP_LOGW("TAG", "Envio latitud por GPRS");
        	    vTaskDelay(10000 / portTICK_PERIOD_MS);
        	//    Envio_GPRS_Lon(gps_data2);
        	    Envio_GPRS(Dato_gps,Campo_lon, gps_data2->longitude_prom);
        	    ESP_LOGW("TAG", "Envio longitud por GPRS");
    			if (puerta_a == 0){
    				puerta_e = 1;
    			}
    	    } else{
    	    	Envio_GPRS(Dato_puerta,Campo_puerta,100.0);
    	    	ESP_LOGI(TAG, "Puerta abierta, sin conexion gps");
    			if (puerta_a == 0){
    				puerta_e = 1;
    			}
    	    }
    	} else{
			gps_data.error_gps = 0;
			Envio_GPRS(Dato_puerta,Campo_puerta,100.0);
			ESP_LOGW(TAG, "2- Puerta abierta, no hay gps");
			if (puerta_a == 0){
				puerta_e = 1;
			}
    	}
    break;
    }
}

void  Enviar_Mensaje(gps_data_t* gps_data2, AM2301_data_t* Thum2){
	//Esta funcion se encarga de enviar el mensaje de texto adecuado
	char message[318] = "Welcome to ESP32!";
	uint8_t lenght = 0;

    if (Thum2->error_temp == 0){
    		if (Thum2->primer_ciclo == 0){
    			switch(limite_b){
    			case 0:
    				sprintf(message,"La humedad es: %.1f %% y la temperatura es: %.1f C",Thum2->Prom_hum[Thum2->pos_temp-1],Thum2->Prom_temp[Thum2->pos_temp-1]);
    				lenght = strlen(message);
    				Envio_mensaje(message,lenght);
    				ESP_LOGI(TAG, "Send send message [%s] ok", message);
    			break;
    			case 1:
    				sprintf(message,"La temperatura se salio de los limites. La humedad es: %.1f  %% y la temperatura es: %.1f C",Thum2->Prom_hum[Thum2->pos_temp-1],Thum2->Prom_temp[Thum2->pos_temp-1]);
    				lenght = strlen(message);
    				Envio_mensaje(message,lenght);
    				ESP_LOGI(TAG, "Send send message [%s] ok", message);
    				if (limite_a == 0){
    					limite_b = 0;
    				}
    				break;
    			}
    		} else {
   				sprintf(message,"No se ha terminado el primer ciclo de la medicion de temperatura.");
   				lenght = strlen(message);
   				Envio_mensaje(message,lenght);
    			ESP_LOGI(TAG, "Send send message [%s] ok", message);
    		}
    } else {
    	Thum.error_temp = 0;
    	sprintf(message,"No se logro medir la temepratura. Revisar las conexiones.");
    	lenght = strlen(message);
    	Envio_mensaje(message,lenght);
    	ESP_LOGI(TAG, "Send send message [%s] ok", message);
    }

    vTaskDelay(200 / portTICK_PERIOD_MS);
    bzero(message,318);

    switch (puerta_b){
    case P_cerrada:
    // Verifico si no hubo error al conectarse a al modulo GPS
        if (gps_data2->error_gps == 0){
        	//Verifico si se conecto a la red GPS viendo si devolvio que es el a;o 2020
        	if (gps_data2->year == 20){
			    sprintf(message,"La latitud es: %.6f %s y la longitud es: %.6f %s",gps_data2->latitude_prom,gps_data2->latitude_direct,gps_data2->longitude_prom,gps_data2->longitude_direct);
			    lenght = strlen(message);
			    Envio_mensaje(message,lenght);
			    ESP_LOGI(TAG, "Send send message [%s] ok", message);
			    bzero(message,318);
        		sprintf(message,"Las medidas se realizaron el %d de %s de 20%d a las %d horas con %d minutos y %d segundos",gps_data2->day,gps_data2->mes,gps_data2->year,gps_data2->hour,gps_data2->minute,gps_data2->second);
        		lenght = strlen(message);
        		Envio_mensaje(message,lenght);
        		ESP_LOGI(TAG, "Send send message [%s] ok", message);
        	} else{

        		 sprintf(message,"No se logro conectar a la red GPS.");
        		 lenght = strlen(message);
        		 Envio_mensaje(message,lenght);
        		 ESP_LOGI(TAG, "Send send message [%s] ok", message);
        	}
        } else{
  			gps_data.error_gps = 0;
    		sprintf(message,"No se logro conectar con el modulo GPS.");
    		lenght = strlen(message);
    		Envio_mensaje(message,lenght);
    		ESP_LOGI(TAG, "Send send message [%s] ok", message);
        }
    break;
    case P_abierta:
    	// Verifico si no hubo error al conectarse a al modulo GPS
    	if (gps_data2->error_gps == 0){
    	//Verifico si se conecto a la red GPS viendo si devolvio que es el a;o 2020
    		if (gps_data2->year == 20){

    			sprintf(message,"La puerta fue abierta. La latitud es: %.6f %s y la longitud es: %.6f %s",gps_data2->latitude_prom,gps_data2->latitude_direct,gps_data2->longitude_prom,gps_data2->longitude_direct);
    			lenght = strlen(message);
    			Envio_mensaje(message,lenght);
    			ESP_LOGI(TAG, "Send send message [%s] ok", message);
    			if (puerta_a == 0){
    				puerta_d = 1;
    			}
    			 bzero(message,318);
    	         sprintf(message,"Las medidas se realizaron el %d de %s de 20%d a las %d horas con %d minutos y %d segundos",gps_data2->day,gps_data2->mes,gps_data2->year,gps_data2->hour,gps_data2->minute,gps_data2->second);
    	         lenght = strlen(message);
    	         Envio_mensaje(message,lenght);
    	         ESP_LOGI(TAG, "Send send message [%s] ok", message);
    	    } else{

    	    	 sprintf(message,"La puerta se abrio, pero no se logro conectar a la red GPS.");
    	    	 lenght = strlen(message);
    	    	 Envio_mensaje(message,lenght);
    	    	 ESP_LOGI(TAG, "Send send message [%s] ok", message);
    			if (puerta_a == 0){
    				puerta_d = 1;
    			}
    	    }
    	} else{
			gps_data.error_gps = 0;
			sprintf(message,"La puerta se abrio, pero no se logro conectar con el modulo GPS.");
			lenght = strlen(message);
			Envio_mensaje(message,64);
			ESP_LOGI(TAG, "Send send message [%s] ok", message);
			if (puerta_a == 0){
				puerta_d = 1;
			}
    	}
    break;
    }
}


void set_form_flash_init( message_data_t *datos){
	//Escribir en la memoria flash
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

    //Si se logro configurar correctamente y activar el GPRS se enviando los datos
    if (ATCOM2 != CPOWD2 && pdp_deact == 0 ){
    	Enviar_GPRS(&gps_data2,&Thum2);
    }

    /*Puerta d y e me indican si ya se mando el mensaje de la puerta por texto y gprs respectivamente
    Luego si la puerta esta cerrada  cambiara puerta d y e a 1 para
    restablecer el valor de las variables para el siguiente caso*/

	if (puerta_d == 1 && puerta_e == 1){
		puerta_b = P_cerrada;
		puerta_c = 0;
		puerta_d = 0;
		puerta_e = 0;
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





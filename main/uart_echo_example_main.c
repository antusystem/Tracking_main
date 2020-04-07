/* UART Echo Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"


//Defino el tamano del buffer de recepcion de datos GPS
//Con el modulo A9G los buses llegan por segundo y tienden a ser un string
//de aproximadament 282 caracteres
#define BUF_SIZE (1024)

#include "NMEA_setting.h"

NMEA_data_t NMEA_data;

//Se definen auxiliares
uint8_t auxi1_echo = 0, auxi2_echo = 0;

//char auxc1_echo[] = "AT\r\n\r\nOK\r\n";
//El siguiente auxiliar se usa para tener otra variable con los datos que llegan al buffer de recepcion
char auxc2_echo[BUF_SIZE] = "";
//char auxc3_echo[] = "AT+GPS=1\r\nOK\r\n";
//char auxc4_echo[] = "AT+GPSRD=1\r\nOK\r\n";
//Los siguientes dos char se ponen ya que no queria agarrarlo la parte donde se usa
//a pesar de usar comillas simples
//char auxc5_echo[1] = "$";
//char auxc7_echo[1] = "B";




//posicion echo es un arreglo para encontrar la posicion de los $ en el bus datos NMEA del GPS
//uint16_t posicion_echo[13] = {0};

//Variables globales para calcular promedios en varias funciones
float prom_lon = 0;
float prom_lat = 0;

//Para saber si es la primera vez que mando AT + GPS = 1
uint8_t primera_vuelta = 0;


#define ECHO_TEST_TXD  (GPIO_NUM_18)
#define ECHO_TEST_RXD  (GPIO_NUM_5)
#define ECHO_TEST_RTS  (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS  (UART_PIN_NO_CHANGE)
int len = 0;

//Para usar gestion de eventos
extern EventGroupHandle_t event_group;

extern const int BEGIN_TASK2;

extern const int SYNC_BIT_TASK2;



//Para la cola

extern QueueHandle_t xQueue_gps;

static const char *TAG1 = "uart_echo_example";

static gps_data_t RMC_parsing(char* GNRMC_data, gps_data_t *GPS_data){

	//Esta funcion ordena los datos de RMC
	// Con la variable ronda (que se aumenta cada vez que llega algo al buffer) se lleva el control
	// de la posicion que se guardara en el arreglo de latitud y longitud. Al llegar a 10 debe parar
	// de enviar datos
	rmc_data_t rmc_data;
	uint16_t flags2[11] = {0};
//	int l1 = 0;
	//Se agrega la hora en la zona horaria
	int zona_horaria = - 4;
	char hora[3] = "";
	char min[3] = "";
	char seg[3] = "";
	char dia[3] = "";
	char mes[3] = "";
	char ano[3] = "";
	//se agrega coma porque poniendo ',' en strstr no agarraba
	char* coma = ",";
	//Para contar cuantas veces se guardan los datos se aumenta la cantidad de ronda
	GPS_data->ronda++;
//	GPS_data->stage++;



//Guardar la posicion de las comas en flags2
	for(int k1 = 0; k1 < 11; k1++){
		char* pos = strstr(GNRMC_data,coma);
		flags2[k1] = pos - GNRMC_data;
	//	ESP_LOGI(TAG2,"flag2 es: %d \r\n",flags2[k1]);
		GNRMC_data[flags2[k1]] = '-';
	}

	for (int k1 = 0; k1 < 9; k1++){
		switch (k1){
		case 0:

			strncpy(rmc_data.time_UTC,GNRMC_data+(flags2[k1]+1),flags2[k1+1]-flags2[k1]-1);
			rmc_data.time_UTC[10] = 0x00;
	//		ESP_LOGI(TAG2,"El tiempo UTC es: %s\r\n",rmc_data.time_UTC);

			// Dividir los datos en arrelgos separados
			strncpy(&hora[0],&rmc_data.time_UTC[0],2);
			hora[2] = 0x00;
			strncpy(&min[0],&rmc_data.time_UTC[2],2);
			min[2] = 0x00;
			strncpy(&seg[0],&rmc_data.time_UTC[4],2);
			seg[2] = 0x00;
	/*		ESP_LOGI(TAG2,"El hora es: %s\r\n",hora);
			ESP_LOGI(TAG2,"El min es: %s\r\n",min);
			ESP_LOGI(TAG2,"El seg es: %s\r\n",seg);*/

			//Guardar los datos en GPS_data
			//A la hora se le suma la zona horaria porque la que da el GPS es UTC
			//En el caso de Venezuela es sola -4 h, si afectara los minutos se debe acomodarlo
			GPS_data->hour = atoi(hora) + zona_horaria;
			GPS_data->minute = atoi(min);
			GPS_data->second = atoi(seg);

			switch (GPS_data->hour){
			case -4:
				GPS_data->hour = 20;
			break;
			case -3:
				GPS_data->hour = 21;
			break;
			case -2:
				GPS_data->hour = 22;
			break;
			case -1:
				GPS_data->hour = 23;
			break;
			}
		/*	ESP_LOGI(TAG2,"El hora es: %d\r\n",GPS_data->hour);
			ESP_LOGI(TAG2,"El min es: %d\r\n",GPS_data->minute);
			ESP_LOGI(TAG2,"El seg es: %d\r\n",GPS_data->second);*/


		break;
		case 1:
		//	l1 = 0;
			strncpy(rmc_data.active,&GNRMC_data[flags2[k1]+1],flags2[k1+1]-flags2[k1]-1);
			rmc_data.active[1] = 0x00;
		//	ESP_LOGI(TAG2,"El estado es: %s\r\n",rmc_data.active);
			strcpy(GPS_data->estado,rmc_data.active);
		//	ESP_LOGI(TAG2,"El estado es: %s\r\n",GPS_data->estado);

		break;
		case 2:
	//		l1 = 0;
			strncpy(rmc_data.latitude,&GNRMC_data[flags2[k1]+1],flags2[k1+1]-flags2[k1]-1);
	//		l1 = strlen(rmc_data.latitude);
			rmc_data.latitude[9] = 0x00;
	//		ESP_LOGI(TAG2,"La latitud en grados y minutos es: %s\r\n",rmc_data.latitude);

			//Pasar a un string los minutos
			strncpy(rmc_data.latitude_min,&rmc_data.latitude[2],7);
	//		l1 = strlen(rmc_data.latitude_min);
			rmc_data.latitude_min[7] = 0x00;
	//		ESP_LOGI(TAG2,"La latitud en minutos es: %s\r\n",rmc_data.latitude_min);

			//Pasar los minutos a float
			rmc_data.latitude_min_f = atof(rmc_data.latitude_min);
	//		ESP_LOGI(TAG2,"La latitud en minutosf es: %f\r\n",rmc_data.latitude_min_f);

			//Pasar a un string los degres
			strncpy(rmc_data.latitude_deg,rmc_data.latitude,2);
	//		l1 = strlen(rmc_data.latitude_deg);
			rmc_data.latitude_deg[2] = 0x00;
	//		ESP_LOGI(TAG2,"La latitud en degres es: %s\r\n",rmc_data.latitude_deg);

			//Pasar los degres a float
			rmc_data.latitude_deg_f = atof(rmc_data.latitude_deg);
	//		ESP_LOGI(TAG2,"La latitud en degf es: %f\r\n",rmc_data.latitude_deg_f);

			//Obtener la longitud en deg y guardolo en GPSdata
	//		float lon1 = (float)rmc_data.latitude_deg_f + (float)(rmc_data.latitude_min_f/60);

			GPS_data->latitude[GPS_data->ronda] = (float)rmc_data.latitude_deg_f + (float)(rmc_data.latitude_min_f/60);
	//		ESP_LOGI(TAG2,"La latitud en DEG es: %f\r\n",GPS_data->latitude[GPS_data->ronda]);

			prom_lat +=  GPS_data->latitude[GPS_data->ronda];
			GPS_data->latitude_prom = GPS_data->latitude[GPS_data->ronda];

	//		ESP_LOGI(TAG2,"El pre -prom lat en DEG es: %f\r\n",prom_lat);

	//		GPS_data->latitude_prom = prom_lat/10;
	//		ESP_LOGI(TAG2,"El promedio de la latitude en DEG es: %f\r\n",GPS_data->latitude_prom);
		break;
		case 3:
			strncpy(rmc_data.latitude_dir,&GNRMC_data[flags2[k1]+1],flags2[k1+1]-flags2[k1]-1);
			rmc_data.latitude_dir[2] = 0x00;

			if(rmc_data.latitude_dir[0] == 'N'){
				sprintf(GPS_data->latitude_direct,"Norte");
			}else{
				sprintf(GPS_data->latitude_direct,"Sur");
			}
		//	ESP_LOGI(TAG2,"La direccion de la latitud es: %s\r\n",GPS_data->latitude_direct);
		break;
		case 4:
//			l1 = 0;
			strncpy(rmc_data.longitude,&GNRMC_data[flags2[k1]+1],flags2[k1+1]-flags2[k1]-1);
//			l1 = strlen(rmc_data.longitude);
			rmc_data.longitude[10] = 0x00;
	//		ESP_LOGI(TAG2,"La longitud en grados y minutos es: %s\r\n",rmc_data.longitude);

			//Pasar a un string los minutos
			strncpy(rmc_data.longitude_min,&rmc_data.longitude[3],7);
	//		l1 = strlen(rmc_data.longitude_min);
			rmc_data.longitude_min[7] = 0x00;
	//		ESP_LOGI(TAG2,"La longitud en minutos es: %s\r\n",rmc_data.longitude_min);

			//Pasar los minutos a float
			rmc_data.longitude_min_f = atof(rmc_data.longitude_min);
	//		ESP_LOGI(TAG2,"La longitud en minutosf es: %f\r\n",rmc_data.longitude_min_f);

			//Pasar a un string los degres
			strncpy(rmc_data.longitude_deg,rmc_data.longitude,4);
	//		l1 = strlen(rmc_data.longitude_deg);
			rmc_data.longitude_deg[3] = 0x00;
	//		ESP_LOGI(TAG2,"La longitud en degres es: %s\r\n",rmc_data.longitude_deg);

			//Pasar los degres a float
			rmc_data.longitude_deg_f = atof(rmc_data.longitude_deg);
	//		ESP_LOGI(TAG2,"La longitud en degf es: %f\r\n",rmc_data.longitude_deg_f);

			//Obtener la longitud en deg y guardolo
			GPS_data->longitude[GPS_data->ronda] = (float)rmc_data.longitude_deg_f + (float)(rmc_data.longitude_min_f/60);
	//		ESP_LOGI(TAG2,"La longitud en DEG es: %f\r\n",GPS_data->longitude[GPS_data->ronda]);

			prom_lon +=  GPS_data->longitude[GPS_data->ronda];
	//		ESP_LOGI(TAG2,"El pre -prom long en DEG es: %f\r\n",prom_lon);
	//		GPS_data->longitude_prom = prom_lon/10;
	//		ESP_LOGI(TAG2,"El promedio de la longitud en DEG es: %f\r\n",GPS_data->longitude_prom);
			GPS_data->longitude_prom = GPS_data->longitude[GPS_data->ronda];


		break;
		case 5:

			strncpy(rmc_data.longitude_dir,&GNRMC_data[flags2[k1]+1],flags2[k1+1]-flags2[k1]-1);
			rmc_data.longitude[1] = 0x00;
	//		ESP_LOGI(TAG2,"La longitud-dir es: %s\r\n",rmc_data.latitude_dir);

			if(rmc_data.longitude_dir[0] == 'E'){
				sprintf(GPS_data->longitude_direct,"Este");
			}else{
				sprintf(GPS_data->longitude_direct,"Oeste");
			}
	//		ESP_LOGI(TAG2,"La direccion de la longitud es: %s\r\n",GPS_data->longitude_direct);
		break;
		case 6:
			strncpy(rmc_data.speed,&GNRMC_data[flags2[k1]+1],flags2[k1+1]-flags2[k1]-1);
			rmc_data.speed[5] = 0x00;
		//	ESP_LOGI(TAG2,"La velocidad es: %s\r\n",rmc_data.speed);

			GPS_data->speed = atof(rmc_data.speed);
		//	ESP_LOGI(TAG2,"La velocidad es: %f\r\n",GPS_data->speed);

		break;
		case 7:

			strncpy(rmc_data.track,&GNRMC_data[flags2[k1]+1],flags2[k1+1]-flags2[k1]-1);
			rmc_data.track[4] = 0x00;
		//	ESP_LOGI(TAG2,"El track es: %s\r\n",rmc_data.track);

			//No agrego track porque no hace falta todavia
	//		GPS_data->track = atof(rmc_data.speed);
	//		ESP_LOGI(TAG2,"La velocidad es: %s\r\n",GPS_data->track);
		break;
		case 8:	//		l1 = 0;
			strncpy(rmc_data.date,&GNRMC_data[flags2[k1]+1],flags2[k1+1]-flags2[k1]-1);
			rmc_data.date[6] = 0x00;
	//		ESP_LOGI(TAG2,"La fecha es: %s\r\n",rmc_data.date);

			// Dividir los datos en arrelgos separados
				strncpy(&dia[0],&rmc_data.date[0],2);
				dia[2] = 0x00;
				strncpy(&mes[0],&rmc_data.date[2],2);
				mes[2] = 0x00;
				strncpy(&ano[0],&rmc_data.date[4],2);
				ano[2] = 0x00;
		/*		ESP_LOGI(TAG2,"El hora es: %s\r\n",hora);
				ESP_LOGI(TAG2,"El min es: %s\r\n",min);
				ESP_LOGI(TAG2,"El seg es: %s\r\n",seg);*/

				//Guardar los datos en GPS_data
				GPS_data->day = atoi(dia);
				GPS_data->month = atoi(mes);
				GPS_data->year = atoi(ano);

				switch (GPS_data->month){
				case 1:
					strcpy(GPS_data->mes,"Enero");
				break;
				case 2:
					strcpy(GPS_data->mes,"Febrero");
				break;
				case 3:
					strcpy(GPS_data->mes,"Marzo");
				break;
				case 4:
					strcpy(GPS_data->mes,"Abril");
				break;
				case 5:
					strcpy(GPS_data->mes,"Mayo");
				break;
				case 6:
					strcpy(GPS_data->mes,"Junio");
				break;
				case 7:
					strcpy(GPS_data->mes,"Julio");
				break;
				case 8:
					strcpy(GPS_data->mes,"Agosto");
				break;
				case 9:
					strcpy(GPS_data->mes,"Septimbre");
				break;
				case 10:
					strcpy(GPS_data->mes,"Octubre");
				break;
				case 11:
					strcpy(GPS_data->mes,"Noviembre");
				break;
				case 12:
					strcpy(GPS_data->mes,"Diciembre");
				break;
				}

			//	ESP_LOGI(TAG2,"La fecha es %d-%d-%d \r\n",GPS_data->day,GPS_data->month,GPS_data->year);
			//	ESP_LOGI(TAG2,"La fecha es %d de %s del 20%d \r\n",GPS_data->day,GPS_data->mes,GPS_data->year);
		break;
		}
	}
	return *GPS_data;
}


static gps_data_t  GPS_parsing(char* data, gps_data_t GPS_data)
{

	//Esta funcion identificara que tipo datos (dentro de los soportados) es la oracion y lo organizara

	char RMC[3] = "RMC";

	if (data[2] == RMC[0] && data[3] == RMC[1] && data[4] == RMC[2]){
	//	ESP_LOGI(TAG2,"Empezara a parsear RMC");
		GPS_data  = RMC_parsing(data, &GPS_data);
	}


	//Se crean las variables auxiliares para comprobar de que tipo son luego se llama
	//la funcion que lo ordenara

	/*	char GSA[3] = "GSA";
		char GSV[3] = "GSV";
		char VTG[3] = "VTG";
		char GGA[3] = "GGA";*/
/*	if (data[2] == GGA[0] && data[3] == GGA[1] && data[4] == GGA[2]){

		ESP_LOGI(TAG2,"Empezara a parsear GGA");
		ESP_LOGI(TAG1,"GNGGA es: %s\r\n",NMEA_data.NMEA_GNGGA);
		GGA_parsing(data, GPS_data);

	}*/
/*
	if (GPGSA_data[2] == GSA[0] && GPGSA_data[3] == GSA[1] && GPGSA_data[4] == GSA[2]){

		ESP_LOGI(TAG2,"Empezara a parsear GSA");

	}

	if (BDGSA_data[2] == GSA[0] && BDGSA_data[3] == GSA[1] && BDGSA_data[4] == GSA[2]){
		ESP_LOGI(TAG2,"Empezara a parsear GSA");

	}

	if (GPGSV_data[2] == GSV[0] && GPGSV_data[3] == GSV[1] && GPGSV_data[4] == GSV[2]){
		ESP_LOGI(TAG2,"Empezara a parsear GSV");

	}

	if (BDGSV_data[2] == GSV[0] && BDGSV_data[3] == GSV[1] && BDGSV_data[4] == GSV[2]){
		ESP_LOGI(TAG2,"Empezara a parsear GSV");

	}
*/

/*
	if (GNVTG_data[2] == VTG[0] && GNVTG_data[3] == VTG[1] && GNVTG_data[4] == VTG[2]){
		ESP_LOGI(TAG2,"Empezara a parsear VTG");

	}
*/
	//Todo lo que esta comentado arriba es por si se piensa implementar para ordenar otras oraciones
	return GPS_data;
}
/*
static NMEA_data_t  NMEA_separator(NMEA_data_t datos_ordenados, char* datos_NMEA, uint16_t leng)
{
	uint16_t posicion[13] = {0};
	NMEA_sentences oracion = 0;
	for(uint8_t k1 = 0; k1 < leng; k1++){
		char* posi = strstr(datos_NMEA,'$');
		posicion[k1] = posi - datos_NMEA;
		ESP_LOGI(TAG2,"posicion es: %d \r\n",posicion[k1]);
		datos_NMEA[posicion[k1]] = '-';
	}

	for (oracion; oracion < 4; oracion++){
		switch (oracion){
		case GNGGA:
			strncpy(datos_NMEA->NMEA_GNGGA,GNRMC_data+(flags2[k1]+1),flags2[k1+1]-flags2[k1]-1);
			ESP_LOGI("PRUEBA","Caso 0.1");
			rmc_data.time_UTC[10] = 0x00;
		break;
		}

	}

	for (int i = posicion_echo[0] + 1; i < (posicion_echo[1]-1); i++){
		NMEA_data.NMEA_GNGGA[i3]= tx_buf[i];
	    i3++;
	}
}
*/


  void echo_task(void *arg)
{
	//Se inicia la tarea configurando los Uart 0 y Uart 2
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
 //   ESP_LOGI(TAG1, "Empezar a configurar Uart 0");
    uart_driver_install(UART_NUM_0, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &uart_config);
    uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, ECHO_TEST_RTS, ECHO_TEST_CTS);

 //   ESP_LOGI(TAG1, "Uart 0 Iniciado");
 //   ESP_LOGI(TAG1, "Empezar a configurar Uart 2");

    uart_param_config(UART_NUM_2, &uart_config);
    uart_set_pin(UART_NUM_2, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS);
    uart_driver_install(UART_NUM_2, BUF_SIZE * 2, 0, 0, NULL, 0); //BOGUS


//    ESP_LOGI(TAG1, "Uart 2 Iniciado");

    //Se declara la variable que copiara lo que llegue al buffer
    //uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
    uint8_t tx_buf[BUF_SIZE];

    //Esta variable sirve para indicar cuando pedir que no siga enviando los datos GPS
    uint8_t * parar_RD1 =  malloc(10);
    parar_RD1 = 0;

    /*//Ronda servira para indicar la ronda en que se encuentra pidiendo los datos GPS
    uint8_t * ronda =  malloc(sizeof( uint8_t));
    *ronda = 0;*/

    //Para encontrar la posicion de los $ en la oracion NMEA
    uint16_t posicion_echo[13] = {0};

    uint8_t len3 = 0;
    uint8_t len5 = 0;
    uint8_t len7 = 0;

    gps_data.ronda_error = 0;
    gps_data.error_gps = 0;
    gps_data.ronda = 0;
    gps_data.stage = 0;



    while (1) {

    	xEventGroupWaitBits(event_group,BEGIN_TASK2,pdFALSE,true,portMAX_DELAY);

    	if (parar_RD1 == 1){
    		parar_RD1 = 0;
    		uart_write_bytes(UART_NUM_2,"AT+GPSRD=0\r\n", 12);
    	}

    	//Se debe limpiar tx_buf para borrar lo de la ronda anterior
   // 	ESP_LOGI(TAG1, "Limpiare tx_buf");
    	bzero(tx_buf,BUF_SIZE);

    	// Empezara escaladamente a mandar lo comandos AT necesarios en cada vuelta

    	if (parar_RD1 == 0){
    	switch (auxi1_echo){
    	case 0:
    		//En la primera vuelta manda AT para ver si la comunicacion ya se puede establecer
	        len3 = uart_write_bytes(UART_NUM_2,"AT\r\n", 4);
	        ESP_LOGI(TAG1, "envio: AT\r\n");
	    break;
    	case 1:
    		//En la segunda vuelta activa el GPS
    		if (primera_vuelta == 0){
        		len5 = uart_write_bytes(UART_NUM_2,"AT+GPS=1\r\n",10);
        		ESP_LOGI(TAG1, "envio: AT+GPS=1\r\n");
    		} else {
    			//Si ya no es la primera vuelta entonces hago esto para que entre en el siguiente if
    			len5 = uart_write_bytes(UART_NUM_2,"AT\r\n", 4);
    			ESP_LOGI(TAG1, "Len5 es 2\r\n");
    		}
    	break;
    	case 2:
    		//En la tercera vuelta solicita los datos del GPS
    		if (auxi2_echo == 0){
    			len7 = uart_write_bytes(UART_NUM_2,"AT+GPSRD=1\r\n", 12);
    			ESP_LOGI(TAG1, "envio: AT+GPSRD=1\r\n");
    		} else {
    			//Si ya no es la primera vuelta entonces hago esto para que entre en el siguiente if
    			len7 = 2;
    		}
		break;
    	}
    	}

    	//Cuando se envia algo len se vuelve la longitud de lo que se envio
    	//si no se envio nada, entonces no estrara en esta parte. Ademas,
    	// len7 no se reinicia para que luego de pedir los datos GPS siempre entre
    	if ((len3 > 0) || (len5 > 0) || (len7 > 0) ) {

    		//Lee el uart
    	//	ESP_LOGI(TAG1, "Voy a leer el uart");
    	    len = uart_read_bytes(UART_NUM_2, (uint8_t*)tx_buf, BUF_SIZE, pdMS_TO_TICKS(10));
    	//    ESP_LOGI(TAG1, "len es: %d",len);


    	    //Se comprueba si llego algo al uart y se publica que llego
    	    if(len>0){
    	     //	ESP_LOGI(TAG1, "Borrare auxc2");
    	     	// Borrar lo que tenia antes auxc2
    	     	bzero(auxc2_echo,BUF_SIZE);
    	     	// Mostar que se borro todo
    	    // 	ESP_LOGI(TAG1, "Se vacio, auxc2 es: %s",auxc2_echo);
    	     	// Copiar lo que esta en el buffer de recepcion


    	     	//Se copia lo que llego al buffer, las primeras 2 veces no hace falta que
    	     	//tenga una dimension tan grande el auxiliar
    	     	if (auxi1_echo < 2){
    	     		memcpy(auxc2_echo,tx_buf,20);
    	     	} else {
    	     		memcpy(auxc2_echo,tx_buf,BUF_SIZE);
    	     	}


    	     	//Mostrar auxc2
    	     	ESP_LOGI(TAG1, "Copio el buffer, auxc2 es: %s",auxc2_echo);
    	     	//printf(" Copio el buffer, auxc2 es: %s",auxc2)
    	 //    	ESP_LOGI(TAG1, " Ya termino auxc2");



    	     	//Empezara a ver escaladamente como comprueba ATOK, ATGPS y "ATGPSRD"


    	        if (parar_RD1 == 0){

    	        	switch (auxi1_echo){
    	        	case 0:
    	        		//En el caso 0 verifica si respondio AT OK
    	        		if (strncmp(auxc2_echo,"AT\r\n\r\nOK\r\n",10) == 0){
    	        			ESP_LOGI(TAG1, "1- Respondio AT OK \r\n");
    	         	        auxi1_echo++;
    	         	        len3 = 0;
    	         	       gps_data.ronda_error = 0;
    	         	    } else {
     	        	    	ESP_LOGI(TAG1, "1- NO respondio AT OK \r\n");
     	        	    	gps_data.ronda_error++;
     	        	    	if (gps_data.ronda_error >= 15){
     	        	    		//Pongo error_gps en 1 para saber que no se logro la comunicacion con el GPS
     	        	    		gps_data.ronda_error = 0;
     	        	    		gps_data.error_gps = 1;
     	        	    		ESP_LOGI(TAG1, "1- GPS error es 1 \r\n");
     	        	    		xEventGroupSetBits(event_group, SYNC_BIT_TASK2);
     	        	    	}
     	        	    }
    	        		break;
    	        	case 1:
    	        		gps_data.ronda_error = 0;
    	        		if (primera_vuelta == 0){
    	        			if (strncmp(auxc2_echo,"AT+GPS=1\r\nOK\r\n",10) == 0){
    	        			   auxi1_echo++;
    	        			   len5 = 0;
    	        			   //Ahora se esperara 40 segundos para que se logre conectar a la red GPS
    	        			   ESP_LOGI(TAG1, "2- Respondio AT+GPS=1 OK \r\n");
    	        		       ESP_LOGI(TAG1, "2- Esperare 40 segundos \r\n");
    	        		       vTaskDelay(pdMS_TO_TICKS(40000));
    	        			}
    	        		} else {
    	        			if (strncmp(auxc2_echo,"AT\r\n\r\nOK\r\n",10) == 0){
	        		    	auxi1_echo++;
	        		    	len5 = 0;
	        				}
	        			}
    	        		break;
    	        		//Este caso causa molestia de vez en cuando y aparte no es necesario
    	        		//para verificar si llegan los datos
    	        	/*	case 2:
      	        	    	if (strncmp(auxc2_echo,auxc4_echo,6) == 0){
        	         	    	ESP_LOGI(TAG1, "3- Respondio AT+GSPRD=1 OK \r\n");
        	         	    	auxi1_echo++;
        	         	    } else {
        	        	    	ESP_LOGI(TAG1, "3- NO respondio AT+GPSRD=1 OK \r\n");
        	        	    }
      	        	    break;*/
    	        	case 2:
    	        	    //Entrara aca despues de mandar AT+GPSRD=1
    	        	    //En este ciclo se encarga de separar las oraciones del bus NMEA del GPS
    	        			ESP_LOGI(TAG1,"Entre en el ultimo ciclooooooooooooooo");
    	        			auxi2_echo = 1;

    	        			int i2 = 0;
    	        			//En este for escanea el buffer por la posicion de $
    	        			for (uint16_t i = 0; i < len; i++){
    	        				if (auxc2_echo[i] == '$' ){
    	        						posicion_echo[i2] = i;
    	        						/*	ESP_LOGI(TAG1,"Posicion [%d] es: %d\r\n",i2,posicion[i2]);
    	        						ESP_LOGI(TAG1,"auxc2 [%d] es: %c\r\n",i2,auxc2[i]);*/
    	        						i2++;
    	        				}
    	        			}
    	        			// Empezare a guardar en el struct escaladamente segun la posicion de $
    	        			// Se guardara cada oracion por separado
    	        			int i3 = 0;
    	        			for (int i = posicion_echo[0] + 1; i < (posicion_echo[1]-1); i++){
    	        				NMEA_data.NMEA_GNGGA[i3]= tx_buf[i];
    	        			    i3++;
    	        			}
    	        			i3 = 0;
    	        			for (int i = posicion_echo[1] + 1; i < (posicion_echo[2]-1); i++){
    	        				NMEA_data.NMEA_GPGSA[i3]= tx_buf[i];
    	        			    i3++;
    	        			}
    	        			i3 = 0;
    	        			for (int i = posicion_echo[2] + 1; i < (posicion_echo[3]-1); i++){
    	        			    NMEA_data.NMEA_BDGSA[i3]= tx_buf[i];
    	        			    i3++;
    	        			}
    	        			i3 = 0;
    	        			for (int i = posicion_echo[3] + 1; i < (posicion_echo[4]-1); i++){
    	        			    NMEA_data.NMEA_GPGSV1[i3]= tx_buf[i];
    	        			    i3++;
    	        			}
    	        			//Cuando se conecte a los satelites mandara de 3 a 12 oraciones GPGSV, por lo que
    	        			// se guarda la primera y se espera los siguientes datos, comprobando que comience por B
    	        			// la siguiente oracion y no por G
    	        			for (int j = 4; j < 10; j++){
    	        			    //j = 4 porque se ve de las siguientes oraciones donde esta la B
    	        			    //para empezar a guardar
    	        			    i3 = 0;
    	        			    if (tx_buf[posicion_echo[j]+1] == 'B'){
    	        			        for (int i = posicion_echo[j] + 1; i < (posicion_echo[j+1]-1); i++){
    	        			        	NMEA_data.NMEA_BDGSV[i3]= tx_buf[i];
    	        			        	i3++;
    	        			        }
    	        			        i3 = 0;
    	        			        for (int i = posicion_echo[j+1] + 1; i < (posicion_echo[j+2]-1); i++){
    	        			        	NMEA_data.NMEA_GNRMC[i3]= tx_buf[i];
    	        			        	i3++;
    	        			        }
    	        			        i3 = 0;
    	        			        for (int i = posicion_echo[j+2] + 1; i < (posicion_echo[j+2]+39); i++){
    	        			        	NMEA_data.NMEA_GNVTG[i3]= tx_buf[i];
    	        			        	i3++;
    	        			        }
    	        			    }
    	        			}

    	        		/*	ESP_LOGI(TAG1,"GPGSA es: %s\r\n",NMEA_data.NMEA_GPGSA);
    	        			ESP_LOGI(TAG1,"BDGSA es: %s\r\n",NMEA_data.NMEA_BDGSA);
    	        			ESP_LOGI(TAG1,"GPGSV1 es: %s\r\n",NMEA_data.NMEA_GPGSV1);
    	        			ESP_LOGI(TAG1,"GNRMC es: %s\r\n",NMEA_data.NMEA_GNRMC);
    	        			ESP_LOGI(TAG1,"GNVTG es: %s\r\n",NMEA_data.NMEA_GNVTG);*/


    	        		//Una vez separadas las oraciones, de mandan a ordenar con la siguiente funcion
    	        		//	GPS_parsing(NMEA_data.NMEA_GNGGA, gps_data);

    	        			ESP_LOGI(TAG1,"GNRMC es: %s\r\n",NMEA_data.NMEA_GNRMC);
    	        			gps_data = GPS_parsing(NMEA_data.NMEA_GNRMC, gps_data);
    	        			ESP_LOGI(TAG1,"ronda es %d", gps_data.ronda);
    	        			xQueueOverwrite(xQueue_gps,&gps_data);
    	        			bzero(NMEA_data.NMEA_GNRMC,256);

    	        			if (primera_vuelta == 0){
    	        				if (gps_data.ronda == 10 ){
    	        					primera_vuelta = 1;
    	        					//Como ya termine de guardar 10 veces los datos reinicio las variables globales
    	        					gps_data.ronda = 0;
    	        				//	bzero(NMEA_data.NMEA_GNRMC,256);
    	        					len7 = 0;
    	        					prom_lat = 0;
    	        					prom_lon = 0;
    	        					auxi1_echo = 0;
    	        					auxi2_echo = 0;
    	        					parar_RD1 = (uint8_t *) 1;
    	        					//Para detener el envio de datos del GPS se manda lo siguiente
    	        					len = uart_write_bytes(UART_NUM_2,"AT+GPSRD=0\r\n", 12);
    	        					len = 0;
    	        					xEventGroupSetBits(event_group, SYNC_BIT_TASK2);
    	        				//	xEventGroupClearBits(event_group, BEGIN_TASK2);
    	        				}
    	        			}

	        				if (gps_data.ronda == 40 ){
	        					//Como ya termine de guardar 10 veces los datos reinicio las variables globales
	        					gps_data.ronda = 0;
	        				//	bzero(NMEA_data.NMEA_GNRMC,256);
	        					len7 = 0;
	        					prom_lat = 0;
	        					prom_lon = 0;
	        					auxi1_echo = 0;
	        					auxi2_echo = 0;
	        					parar_RD1 = (uint8_t *) 1;
	        					//Para detener el envio de datos del GPS se manda lo siguiente
	        					len = uart_write_bytes(UART_NUM_2,"AT+GPSRD=0\r\n", 12);
	        					len = 0;
	        					xEventGroupSetBits(event_group, SYNC_BIT_TASK2);
	        				//	xEventGroupClearBits(event_group, BEGIN_TASK2);
	        				}

    	        		    break;
    	        		}
    	        	}

    	        	if (parar_RD1 == 1){
    	        		 ESP_LOGI(TAG1, "4- Para RD es true \r\n");
    	        		 vTaskDelay( pdMS_TO_TICKS(3000) );
    	        	}

    	        	//Espera un segundo que es el tiempo optimo de espera para solicitar los datos
	        	    vTaskDelay( pdMS_TO_TICKS(1000) );
    	     		ESP_LOGI(TAG1, " 1+ Espere 1 segundos");
    	     	}
    	}

    	//Entra aqui si no se envio nada
    	if (len <= 0){
    		vTaskDelay( pdMS_TO_TICKS(1000) );
    		ESP_LOGI(TAG1, "2+ Espere 1 segundo");
    		gps_data.ronda_error++;
    		if (gps_data.ronda_error >= 25){
    			//Pongo error_gps en 1 para saber que no se logro la comunicacion con el GPS
    			gps_data.ronda_error = 0;
    			gps_data.error_gps = 1;
    			xQueueOverwrite(xQueue_gps,&gps_data);
    			ESP_LOGI(TAG1, "1- GPS error es 1 \r\n");
    			xEventGroupSetBits(event_group, SYNC_BIT_TASK2);
    		}

    	}
    }
}




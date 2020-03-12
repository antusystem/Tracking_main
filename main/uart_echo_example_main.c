/* UART Echo Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "driver/uart.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"

#define BUF_SIZE (1024)

#include "NMEA_setting.h"

NMEA_data_t NMEA_data;

extern const char *nvs_tag;

int auxi1_echo = 0;
int auxi2_echo = 0;
int auxi3_echo = 0;
int ronda = 0;



char auxc1_echo[] = "AT\r\n\r\nOK\r\n";
char auxc2_echo[BUF_SIZE] = "";
char auxc3_echo[] = "AT+GPS=1\r\nOK\r\n";
char auxc4_echo[] = "AT+GPSRD=1\r\nOK\r\n";
char auxc5_echo[1] = "$";
char auxc6_echo[BUF_SIZE] = "";
char auxc7_echo[1] = "B";

uint16_t posicion_echo[13] = {0};


float prom_lon = 0;
float prom_lat = 0;


#define ECHO_TEST_TXD  (GPIO_NUM_18)
#define ECHO_TEST_RXD  (GPIO_NUM_5)
#define ECHO_TEST_RTS  (UART_PIN_NO_CHANGE)
#define ECHO_TEST_CTS  (UART_PIN_NO_CHANGE)
int len = 0;

//Para usar gestion de eventos
extern EventGroupHandle_t event_group;

extern const int BEGIN_TASK1;

extern const int BEGIN_TASK2;

extern const int BEGIN_TASK3;




static const char *TAG1 = "uart_echo_example";

void set_form_flash_init1(gps_data_t *GPS_data){
	esp_err_t err;
	nvs_handle_t ctrl_flash;
	err = nvs_open("storage2",NVS_READWRITE,&ctrl_flash);
	if (err != ESP_OK) {
		printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
	}else{
		ESP_LOGI("NVS_FLASH","Latitud a guardar en flash: %s",GPS_data->latitude_s);
		nvs_set_str(ctrl_flash,"latitude_s",GPS_data->latitude_s);

		ESP_LOGI("NVS_FLASH","Longitud a guardar en flash: %s",GPS_data->longitude_s);
		nvs_set_str(ctrl_flash,"longitude_s",GPS_data->longitude_s);

		ESP_LOGI("NVS_FLASH","Latitude direct a guardar en flash: %s",GPS_data->latitude_direct);
		nvs_set_str(ctrl_flash,"latitude_direct",GPS_data->latitude_direct);

		ESP_LOGI("NVS_FLASH","Longitude direct a guardar en flash: %s",GPS_data->longitude_direct);
		nvs_set_str(ctrl_flash,"logitude_direct",GPS_data->longitude_direct);

		err = nvs_commit(ctrl_flash);
	}
	nvs_close(ctrl_flash);

}

void get_form_flash1(gps_data_t *GPS_data){
	size_t leni;
	esp_err_t err;
	nvs_handle_t ctrl_flash;

	err = nvs_open("storage2",NVS_READWRITE,&ctrl_flash);
	if (err != ESP_OK) {
		printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
	}else{

		err = nvs_get_str(ctrl_flash,"latitude_s",NULL,&leni);
		if(err==ESP_OK) {
			err = nvs_get_str(ctrl_flash,"latitude_s",GPS_data->latitude_s,&leni);
			switch(err){
				case ESP_OK:
					ESP_LOGI(nvs_tag,"latitude en flash: %s",GPS_data->latitude_s);
				break;
				case ESP_ERR_NVS_NOT_FOUND:
					ESP_LOGI(nvs_tag,"latitude en flash: none");
				break;
				default:
					printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
				break;
			}
		}

		err = nvs_get_str(ctrl_flash,"latitude_direct",NULL,&leni);
		if(err==ESP_OK){
			err= nvs_get_str(ctrl_flash,"latitude_direct",GPS_data->latitude_direct,&leni);
		switch(err){
			case ESP_OK:
				ESP_LOGI(nvs_tag,"latitude_direct en flash: %s",GPS_data->latitude_direct);
			break;
			case ESP_ERR_NVS_NOT_FOUND:
				ESP_LOGI(nvs_tag,"latitude_direct en flash: none");
			break;
			default:
				printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
			break;
		}
		}

		err = nvs_get_str(ctrl_flash,"longitude_s",NULL,&leni);
		if(err==ESP_OK){
			err= nvs_get_str(ctrl_flash,"longitude_s",GPS_data->longitude_s,&leni);
		switch(err){
			case ESP_OK:
				ESP_LOGI(nvs_tag,"longitude en flash: %s",GPS_data->longitude_s);
			break;
			case ESP_ERR_NVS_NOT_FOUND:
				ESP_LOGI(nvs_tag,"longitude en flash: none");
			break;
			default:
				printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
			break;
		}
		}
	}nvs_close(ctrl_flash);

	err = nvs_open("storage2",NVS_READWRITE,&ctrl_flash);
	if (err != ESP_OK) {
		printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
	}else{

		err = nvs_get_str(ctrl_flash,"longitude_direct",NULL,&leni);
		if(err==ESP_OK){
			err= nvs_get_str(ctrl_flash,"longitude_direct",GPS_data->longitude_direct,&leni);
		switch(err){
			case ESP_OK:
				ESP_LOGI(nvs_tag,"longitude_direct en flash: %s",GPS_data->longitude_direct);
			break;
			case ESP_ERR_NVS_NOT_FOUND:
				ESP_LOGI(nvs_tag,"longitude_direct en flash: none");
			break;
			default:
				printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
			break;
		}
		}
	}nvs_close(ctrl_flash);



}


/*
static void GGA_parsing(char* GNGGA_data, gps_data_t GPS_data )
{

	//Esta funcion ordena los datos GGA en el struct GPS_data usando de intemedio gga_data

	gga_data_t gga_data;
	uint8_t flags1[14] = {0};
	int k = strlen(GNGGA_data);
	int k4 = 0;
	char k2[1] = ",";
	int k9 = 0;

	// Se identifica la posicion de las comas para luego guardar por partes lo datos
	for (int k1 = 0; k1 <= k; k1++ ){
		//ESP_LOGI(TAG2,"Entre al ciclo, k1 es: %d \r\n  k es: %d\r\n y k2 es: %s \r\n y GNGGAdata[k1] es: %c",k1,k,k2,GNGGA_data[k1]);
		if (GNGGA_data[k1] == k2[0]){
			flags1[k4] = k1;
		//	ESP_LOGI(TAG2,"La coma esta en: %d \r\n", flags1[k4]);
			k4++;
		}
	}

	//Guardar datos Time UTC

	k4 = 0;
	for (int k1 = flags1[0]+1; k1 < (flags1[1]); k1++){
		gga_data.time_UTC[k4] = GNGGA_data[k1];
		k4++;

	}
	gga_data.time_UTC[k4] = 0x00;
	//Se debe anadir 0x00 para que tenga fin el string

//	ESP_LOGI(TAG2,"1- La hora UTC es: %s \r\n", gga_data.time_UTC);

	//Guardar datos de latitud

	k4 = 0;
	for (int k1 = flags1[1]+1; k1 < (flags1[2]); k1++){
		gga_data.latitude[k4] = GNGGA_data[k1];
		k4++;
	}
	gga_data.latitude[k4] = 0x00;


	ESP_LOGI(TAG2,"1- La latitud en grados y minutos es: %s \r\n", gga_data.latitude);

	//ordenar los datos para que sean float
	k9 = strlen(gga_data.latitude);
	gga_data.latitude_min[8] = 0x00;
	gga_data.latitude_min[7] = gga_data.latitude[k9];
	gga_data.latitude_min[6] = gga_data.latitude[k9-1];
	gga_data.latitude_min[5] = gga_data.latitude[k9-2];
	gga_data.latitude_min[4] = gga_data.latitude[k9-3];
	gga_data.latitude_min[3] = gga_data.latitude[k9-4];
	gga_data.latitude_min[2] = gga_data.latitude[k9-5];
	gga_data.latitude_min[1] = gga_data.latitude[k9-6];
	gga_data.latitude_min[0] = gga_data.latitude[k9-7];

//	ESP_LOGI(TAG2,"1- La latitud min es: %s \r\n", gga_data.latitude_min);

	gga_data.latitude_min_f = atof(gga_data.latitude_min);
//	ESP_LOGI(TAG2, "latitude min f es: %f",gga_data.latitude_min_f);

	gga_data.latitude_deg_f = (atof(gga_data.latitude) - atof(gga_data.latitude_min))/100;
//	ESP_LOGI(TAG2, "latitude deg f es: %f",gga_data.latitude_deg_f);

	sprintf(gga_data.latitude_deg, "%f",gga_data.latitude_deg_f);

//	ESP_LOGI(TAG2,"1- La latitud deg en grados es: %s \r\n", gga_data.latitude_deg);

	float lat1 = gga_data.latitude_deg_f + gga_data.latitude_min_f/60;

	//sprintf(gga_data.latitude,"%f",lat1);


	gga_data.latitude_dir[0] = GNGGA_data[flags1[3]-1];
	gga_data.latitude_dir[1] = 0x00;


	ESP_LOGI(TAG2,"1- La direccion de la latitud es: %s \r\n", gga_data.latitude_dir);

	k4 = 0;
	for (int k1 = flags1[3]+1; k1 < (flags1[4]); k1++){
		gga_data.longitude[k4] = GNGGA_data[k1];
		k4++;
	}
	gga_data.longitude[k4] = 0x00;

	ESP_LOGI(TAG2,"1- La longitud es: %s \r\n", gga_data.longitude);

	k9 = strlen(gga_data.longitude);
	gga_data.longitude_min[8] = 0x00;
	gga_data.longitude_min[7] = gga_data.longitude[k9];
	gga_data.longitude_min[6] = gga_data.longitude[k9-1];
	gga_data.longitude_min[5] = gga_data.longitude[k9-2];
	gga_data.longitude_min[4] = gga_data.longitude[k9-3];
	gga_data.longitude_min[3] = gga_data.longitude[k9-4];
	gga_data.longitude_min[2] = gga_data.longitude[k9-5];
	gga_data.longitude_min[1] = gga_data.longitude[k9-6];
	gga_data.longitude_min[0] = gga_data.longitude[k9-7];

//	ESP_LOGI(TAG2,"1- La longitud min es: %s \r\n", gga_data.longitude_min);
	gga_data.longitude_min_f = atof(gga_data.longitude_min);
	gga_data.longitude_deg_f = (atof(gga_data.longitude) - atof(gga_data.longitude_min))/100;

//	ESP_LOGI(TAG2,"1- La longitud degf es: %f \r\n", gga_data.longitude_deg_f);
	sprintf(gga_data.longitude_deg, "%f",gga_data.longitude_deg_f);

	float lat2 = gga_data.longitude_deg_f + gga_data.longitude_min_f/60;

	//sprintf(gga_data.longitude,"%f",lat1);

	gga_data.longitude_dir[0] = GNGGA_data[flags1[5]-1];
	gga_data.longitude_dir[1] = 0x00;

	ESP_LOGI(TAG2,"1- La direccion de la longitud es: %s \r\n", gga_data.longitude_dir);


	//Guardar el fix data
	gga_data.fix[0] = GNGGA_data[flags1[6]-1];
	gga_data.fix[1] = 0x00;

	ESP_LOGI(TAG2,"El fix es: %s \r\n", gga_data.fix);

	//Guadar los satelites en uso
	k4 = 0;
	for (int k1 = flags1[6]+1; k1 < (flags1[7]); k1++){
		gga_data.sats_in_use[k4] = GNGGA_data[k1];
		k4++;
	}
	gga_data.sats_in_use[k4] = 0x00;

	ESP_LOGI(TAG2,"Sat in use es: %s \r\n", gga_data.sats_in_use);

	//Guardar horizontal dilution position
	k4 = 0;
	for (int k1 = flags1[7]+1;  k1 < (flags1[8]); k1++){
		gga_data.dop_h[k4] = GNGGA_data[k1];
		k4++;
	}
	gga_data.dop_h[k4] = 0x00;

	ESP_LOGI(TAG2,"El doph es: %s \r\n", gga_data.dop_h);

	//Guardar la altitud
	k4 = 0;
	for (int k1 = flags1[8]+1; k1 < (flags1[9]); k1++){
		gga_data.altitude[k4] = GNGGA_data[k1];
		k4++;
	}
	gga_data.altitude[k4] = 0x00;

	ESP_LOGI(TAG2,"Altitud es: %s \r\n", gga_data.altitude);


	//Usando los auxiliares lat1 y lat2 se guarda la latitud y longitud con su direccion en la variable gps_data
	GPS_data.latitude = lat1;
	if(gga_data.latitude_dir[0] == 'N'){
		sprintf(GPS_data.latitude_direct,"Norte");
	}else{
		sprintf(GPS_data.latitude_direct,"Sur");
	}
	if(gga_data.longitude_dir[0] == 'E'){
		sprintf(GPS_data.longitude_direct,"Este");
	}
	if(gga_data.longitude_dir[0] == 'W'){
		sprintf(GPS_data.longitude_direct,"Oeste");
	}

	ESP_LOGI(TAG2,"La latitud en grados es: %f con direccion: %s \r\n", GPS_data.latitude, GPS_data.latitude_direct);
	GPS_data.longitude = lat2;
	ESP_LOGI(TAG2,"La longitud en grados es: %f con direccion: %s \r\n", GPS_data.longitude, GPS_data.longitude_direct);

}

*/

static gps_data_t RMC_parsing(char* GNRMC_data, gps_data_t *GPS_data ){

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
	//Para calcular un promedio final de los valores se agrega prom de auxiliar

	//Para contar cuantas veces se guardan los datos se aumenta la cantidad de ronda
	ronda++;


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
	//		l1 = 0;
			strncpy(rmc_data.time_UTC,GNRMC_data+(flags2[k1]+1),flags2[k1+1]-flags2[k1]-1);
	//		l1 = strlen(rmc_data.time_UTC);
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
			ESP_LOGI(TAG2,"El hora es: %d\r\n",GPS_data->hour);
			ESP_LOGI(TAG2,"El min es: %d\r\n",GPS_data->minute);
			ESP_LOGI(TAG2,"El seg es: %d\r\n",GPS_data->second);


		break;
		case 1:
		//	l1 = 0;
			strncpy(rmc_data.active,&GNRMC_data[flags2[k1]+1],flags2[k1+1]-flags2[k1]-1);
			rmc_data.active[1] = 0x00;
		//	ESP_LOGI(TAG2,"El estado es: %s\r\n",rmc_data.active);
			strcpy(GPS_data->estado,rmc_data.active);
			ESP_LOGI(TAG2,"El estado es: %s\r\n",GPS_data->estado);

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

			GPS_data->latitude[ronda] = (float)rmc_data.latitude_deg_f + (float)(rmc_data.latitude_min_f/60);
			ESP_LOGI(TAG2,"La latitud en DEG es: %f\r\n",GPS_data->latitude[ronda]);
			ESP_LOGI(TAG2,"La ronda es: %d\r\n",ronda);

			prom_lat +=  GPS_data->latitude[ronda];
			ESP_LOGI(TAG2,"El pre -prom long en DEG es: %f\r\n",prom_lat);

			if (ronda == 10){
				GPS_data->latitude_prom = prom_lat/10;
				sprintf(GPS_data->latitude_s,"%.4f",GPS_data->latitude_prom);
				ESP_LOGI(TAG2,"El promedio de la latitude en DEG es: %f\r\n",GPS_data->latitude_prom);
			}

		break;
		case 3:
//			l1 = 0;
			strncpy(rmc_data.latitude_dir,&GNRMC_data[flags2[k1]+1],flags2[k1+1]-flags2[k1]-1);
//			l1 = strlen(rmc_data.latitude_dir);
			rmc_data.latitude_dir[2] = 0x00;

			if(rmc_data.latitude_dir[0] == 'N'){
				sprintf(GPS_data->latitude_direct,"Norte");
			}else{
				sprintf(GPS_data->latitude_direct,"Sur");
			}
			ESP_LOGI(TAG2,"La direccion de la latitud es: %s\r\n",GPS_data->latitude_direct);
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
			GPS_data->longitude[ronda] = (float)rmc_data.longitude_deg_f + (float)(rmc_data.longitude_min_f/60);
			ESP_LOGI(TAG2,"La longitud en DEG es: %f\r\n",GPS_data->longitude[ronda]);

			prom_lon +=  GPS_data->longitude[ronda];
			ESP_LOGI(TAG2,"El pre -prom long en DEG es: %f\r\n",prom_lon);

			if (ronda == 10){
				GPS_data->longitude_prom = prom_lon/10;
				sprintf(GPS_data->longitude_s,"%.4f",GPS_data->longitude_prom);
				ESP_LOGI(TAG2,"El promedio de la longitud en DEG es: %f\r\n",GPS_data->longitude_prom);
			}



		break;
		case 5:
	//		l1 = 0;
			strncpy(rmc_data.longitude_dir,&GNRMC_data[flags2[k1]+1],flags2[k1+1]-flags2[k1]-1);
	//		l1 = strlen(rmc_data.latitude_dir);
			rmc_data.longitude[1] = 0x00;
	//		ESP_LOGI(TAG2,"La longitud-dir es: %s\r\n",rmc_data.latitude_dir);

			if(rmc_data.longitude_dir[0] == 'E'){
				sprintf(GPS_data->longitude_direct,"Este");
			}else{
				sprintf(GPS_data->longitude_direct,"Oeste");
			}
			ESP_LOGI(TAG2,"La direccion de la longitud es: %s\r\n",GPS_data->longitude_direct);
		break;
		case 6:
//			l1 = 0;
			strncpy(rmc_data.speed,&GNRMC_data[flags2[k1]+1],flags2[k1+1]-flags2[k1]-1);
//			l1 = strlen(rmc_data.speed);
			rmc_data.speed[5] = 0x00;
		//	ESP_LOGI(TAG2,"La velocidad es: %s\r\n",rmc_data.speed);

			GPS_data->speed = atof(rmc_data.speed);
			ESP_LOGI(TAG2,"La velocidad es: %f\r\n",GPS_data->speed);

		break;
		case 7:
	//		l1 = 0;
			strncpy(rmc_data.track,&GNRMC_data[flags2[k1]+1],flags2[k1+1]-flags2[k1]-1);
	//		l1 = strlen(rmc_data.track);
			rmc_data.track[4] = 0x00;
			ESP_LOGI(TAG2,"El track es: %s\r\n",rmc_data.track);

			//No agrego track porque no hace falta todavia
	//		GPS_data->track = atof(rmc_data.speed);
	//		ESP_LOGI(TAG2,"La velocidad es: %s\r\n",GPS_data->track);
		break;
		case 8:
	//		l1 = 0;
			strncpy(rmc_data.date,&GNRMC_data[flags2[k1]+1],flags2[k1+1]-flags2[k1]-1);
	//		l1 = strlen(rmc_data.date);
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

				ESP_LOGI(TAG2,"La fecha es %d-%d-%d \r\n",GPS_data->day,GPS_data->month,GPS_data->year);
				ESP_LOGI(TAG2,"La fecha es %d de %s del 20%d \r\n",GPS_data->day,GPS_data->mes,GPS_data->year);
		break;
		}
	}
	return *GPS_data;
}


static gps_data_t  GPS_parsing(char* data, gps_data_t GPS_data)
{

/*	char GSA[3] = "GSA";
	char GSV[3] = "GSV";
	char VTG[3] = "VTG";
	char GGA[3] = "GGA";*/
	char RMC[3] = "RMC";


	//Se crean las variables auxiliares para comprobar de que tipo son luego se llama la funcion que lo ordenara

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
	if (data[2] == RMC[0] && data[3] == RMC[1] && data[4] == RMC[2]){
		ESP_LOGI(TAG2,"Empezara a parsear RMC");
		GPS_data  = RMC_parsing(data, &GPS_data);
	}
/*
	if (GNVTG_data[2] == VTG[0] && GNVTG_data[3] == VTG[1] && GNVTG_data[4] == VTG[2]){
		ESP_LOGI(TAG2,"Empezara a parsear VTG");

	}
*/

	return GPS_data;
}



  void echo_task(void *arg)
{

    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };
    ESP_LOGI(TAG1, "Empezar a configurar Uart 0");
    uart_driver_install(UART_NUM_0, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_param_config(UART_NUM_0, &uart_config);
    uart_set_pin(UART_NUM_0, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE, ECHO_TEST_RTS, ECHO_TEST_CTS);

    ESP_LOGI(TAG1, "Uart 0 Iniciado");
    ESP_LOGI(TAG1, "Empezar a configurar Uart 2");

    uart_param_config(UART_NUM_2, &uart_config);
    uart_set_pin(UART_NUM_2, ECHO_TEST_TXD, ECHO_TEST_RXD, ECHO_TEST_RTS, ECHO_TEST_CTS);
    uart_driver_install(UART_NUM_2, BUF_SIZE * 2, 0, 0, NULL, 0); //BOGUS


    ESP_LOGI(TAG1, "Uart 2 Iniciado");

    // Configure a temporary buffer for the incoming data
    //uint8_t *data = (uint8_t *) malloc(BUF_SIZE);
    uint8_t tx_buf[BUF_SIZE];


    //Declaro la variable booleana false por si para cuando vuelva a entrar a esta tarea
    uint8_t * parar_RTC1 = (uint8_t * ) malloc(10);
    parar_RTC1 = 0;

    //variable para reiniciar el A9G si no responde AT OK en 10 intentos
    uint8_t reinicio = 0;

    //variable para ignorar el gps
    uint8_t reinicio2 = 0;


    // char com[3] = {0x41, 0x54, 0x0D};
    char *com = {"AT\r\n"};
    int len2 =  4;
    int len3 = 0;

    char *com2 = "AT+GPS=1\r\n";
    int len4 = 10;
    int len5 = 0;

    char *com3 = "AT+GPSRD=1\r\n";
    int len6 = 12;
    int len7 = 0;

    char *com4 = "AT+RST=1\r\n";
    int len8 = 10;



    while (1) {

    	xEventGroupWaitBits(event_group,BEGIN_TASK3,false,true,portMAX_DELAY);

    	if (parar_RTC1 == 1){
    		parar_RTC1 = 0;
    	}

    	//Se debe limpiar tx_buf para borrar lo de la ronda anterior
    	ESP_LOGI(TAG1, "Limpiare tx_buf");
    	bzero(tx_buf,BUF_SIZE);

    	// Empezare escaladamente a mandar lo comandos AT necesarios en cada vuelta

    	//Vuelta 3 pidiendo que envie los datos del GPS
    	if (auxi2_echo == 1 && auxi3_echo == 0 && parar_RTC1 == 0){

    	        len7 = uart_write_bytes(UART_NUM_2,com3, len6);
    	        ESP_LOGI(TAG1, "envio: %s",com3);
    	        auxi3_echo = 1 ;
    	}

    	//Vuelta 2 prendiendo el GPS
    	if (auxi1_echo == 1 && auxi2_echo == 0 && parar_RTC1 == 0){

    	        len5 = uart_write_bytes(UART_NUM_2,com2, len4);
    	        ESP_LOGI(TAG1, "envio: %s",com2);
    	}
    	//Vuelta 1 revisando si hay comunicacion con el modulo A9G
    	if (auxi1_echo == 0 && parar_RTC1 == 0){

    	        len3 = uart_write_bytes(UART_NUM_2,com, len2);
    	        ESP_LOGI(TAG1, "envio: %s",com);
    	}


    	//Cuando se envia algo len se vuelve la longitud d elo que se envio
    	//si no se envio nada, entonces no estrara en esta parte. Ademas,
    	// len7 no se reinicia para que luego de pedir los datos GPS siempre entre
    	if ((len3 > 0) || (len5 > 0) || (len7 > 0) ) {

    		//Lee el uart
    			ESP_LOGI(TAG1, "Voy a leer el uart");
    	     	len = uart_read_bytes(UART_NUM_2, (uint8_t*)tx_buf, BUF_SIZE, pdMS_TO_TICKS(10));
    	     	ESP_LOGI(TAG1, "len es: %d",len);

    	     	//Se comprueba si llego algo al uart y se publica que llego
    	     	if(len>0){
    	     		ESP_LOGI(TAG1, "Borrare auxc2");
    	     		// Borrar lo que tenia antes auxc2
    	     		bzero(auxc2_echo,BUF_SIZE);
    	     		// Mostar que se borro todo
    	     		ESP_LOGI(TAG1, "Se vacio, auxc2 es: %s",auxc2_echo);
    	     		// Copiar lo que esta en el buffer de recepcion

    	     		if (auxi3_echo == 0){
    	     			memcpy(auxc2_echo,tx_buf,20);
    	     		} else {
    	     			memcpy(auxc2_echo,tx_buf,BUF_SIZE);
    	     		}


    	     		//Mostrar auxc2
    	     		ESP_LOGI(TAG1, "Copio el buffer, auxc2 es: %s",auxc2_echo);
    	     		//printf(" Copio el buffer, auxc2 es: %s",auxc2)
    	     		ESP_LOGI(TAG1, " Ya termino auxc2");
    	     		ESP_LOGI(TAG1, "ronda es: %d",ronda);


    	     		//Empezare a ver escaladamente como comprueba ATOK, ATGPS y ATGPSRD


    	     		//Entrara aca despues de mandar AT+GPSRD=1
    	     		//En este ciclo se encarga de separar las oraciones del bus NMEA del GPS
    	     		if (auxi3_echo == 1 && parar_RTC1 == 0){
    	     			ESP_LOGI(TAG1,"Entre en el ultimo ciclooooooooooooooo");

       	     			int i2 = 0;
        	     			//En este for escanea el buffer por la posicion de $
        	     			for (uint16_t i = 0; i < len; i++){
        	     				if (auxc2_echo[i] == auxc5_echo[0] ){
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
    	     			//j = 4 porque se ve de las siguientes oraciones donde esta la B para empezar a guardar
    	     			i3 = 0;
    	     			if (tx_buf[posicion_echo[j]+1] == auxc7_echo[0]){
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

    	     			ESP_LOGI(TAG1,"GPGSA es: %s\r\n",NMEA_data.NMEA_GPGSA);
    	     			ESP_LOGI(TAG1,"BDGSA es: %s\r\n",NMEA_data.NMEA_BDGSA);
    	     			ESP_LOGI(TAG1,"GPGSV1 es: %s\r\n",NMEA_data.NMEA_GPGSV1);
    	     			ESP_LOGI(TAG1,"GNRMC es: %s\r\n",NMEA_data.NMEA_GNRMC);
    	     			ESP_LOGI(TAG1,"GNVTG es: %s\r\n",NMEA_data.NMEA_GNVTG);


    	     			//Una vez separadas las oraciones, de mandan a ordenar con la siguiente funcion
    	     	//	GPS_parsing(NMEA_data.NMEA_GNGGA, gps_data);
    	     		ESP_LOGI(TAG1,"GNRMC es: %s\r\n",NMEA_data.NMEA_GNRMC);

    	     		gps_data = GPS_parsing(NMEA_data.NMEA_GNRMC, gps_data);
    	     		auxi3_echo = 1;
    	     		//vTaskDelay( pdMS_TO_TICKS(500) );
    	     		if (ronda == 10 ){
    	     			//Como ya termine de guardar 10 veces los datos reinicio las variables globales
    					ronda = 0;
    					len7 = 0;
    					prom_lat = 0;
    					prom_lon = 0;
    					auxi1_echo = 0;
    					auxi2_echo = 0;
    					auxi3_echo = 0;
    					parar_RTC1 = (uint8_t *) 1;
    					set_form_flash_init1(&gps_data);
    					get_form_flash1(&gps_data2);
    					//Tambien para el envio de datos del GPSRD
    					len = uart_write_bytes(UART_NUM_2,"AT+GPSRD=0\r\n", 12);
    					len = 0;
    				    xEventGroupClearBits(event_group, BEGIN_TASK3);
    				    xEventGroupSetBits(event_group, BEGIN_TASK2);
    	     		}
    	     	}






    	     		/*//Entrare aca cuando mande AT+GPSRD = 1
    	     		 // Fue omitida ya que a veces no respondia a tiempo OK por lo no cambiaba auxi3
    	     		  // y daba errores
    	        	if (auxi2 == 1 && auxi3 == 0 ){

    	        	    	if (strncmp(auxc2,auxc4,6) == 0){
    	         	    	   ESP_LOGI(TAG1, "3- Respondio AT+GSPRD=1 OK \r\n");
    	         	    	   auxi3 = 1 ;
    	         	        }
    	        	    	if (auxi2 == 0){
    	        	    	   ESP_LOGI(TAG1, "3- NO respondio AT+GPSRD=1 OK \r\n");
    	        	    	}
    	        	} */


    	        	//Verificar si ya dijo AT GPS OK
    	        	if (auxi1_echo == 1 && auxi2_echo == 0 && parar_RTC1 == 0){

    	        	    	if (strncmp(auxc2_echo,auxc3_echo,10) == 0){
    	         	    	   ESP_LOGI(TAG1, "2- Respondio AT+GPS=1 OK \r\n");
    	         	    	   auxi2_echo = 1;
    	         	    	   len5 = 0;
    	         	        }
    	        	    	 if (auxi2_echo == 0){
    	        	    	   ESP_LOGI(TAG1, "2- NO respondio AT+GPS=1 OK \r\n");
    	        	    	 }
    	        	}
    	     		//Verificar si ya dijo AT OK
    	        	if (auxi1_echo == 0 && parar_RTC1 == 0){
    	        	       if (strncmp(auxc2_echo,auxc1_echo,10) == 0){
    	        	    	   ESP_LOGI(TAG1, "1- Respondio AT OK \r\n");
    	        	    	   auxi1_echo = 1;
    	        	    	   len3 = 0;
    	        	       }
    	        	       if (auxi1_echo == 0 && parar_RTC1 == 0){
    	        	    	   ESP_LOGI(TAG1, "1- NO respondio AT OK \r\n");
    	        	    	   reinicio++;
    	        	    	   ESP_LOGI(TAG1, "1- Reinicio es: %d", reinicio);
    	        	    	   if (reinicio == 11){
    	        	    		   reinicio2++;
    	        	    		   reinicio = 0;
    	        	    		   ESP_LOGI(TAG1, "1- Reinicio2 es: %d", reinicio2);
    	        	    		   len = uart_write_bytes(UART_NUM_2,com4,len8);
    	        	    		   len = 0;
    	        	    	   }
    	        	    	   if (reinicio2 == 2){
    	        	    		   xEventGroupClearBits(event_group, BEGIN_TASK3);
    	        	    		   xEventGroupSetBits(event_group, BEGIN_TASK2);
    	        	    	   }
    	        	       }

    	        	}
	        	       if ( parar_RTC1 == 1){
	        	    	   ESP_LOGI(TAG1, "4- Para RTC es true \r\n");
	        	    	   vTaskDelay( pdMS_TO_TICKS(3000) );
	        	       }

	        	    vTaskDelay( pdMS_TO_TICKS(1000) );
    	     		ESP_LOGI(TAG1, " 1+ Espere 1 segundos");
    	     	}
    	}

    	if (len <= 0){
    		reinicio++;
	    	if (reinicio == 11){
	        	    reinicio2++;
	        	    reinicio = 0;
	        	    ESP_LOGI(TAG1, "1- Reinicio2 es: %d", reinicio2);
	        	    len = uart_write_bytes(UART_NUM_2,com4,len8);
	        	    len = 0;
	        }
	        if (reinicio2 == 2){
	        	    xEventGroupClearBits(event_group, BEGIN_TASK3);
	        	    xEventGroupSetBits(event_group, BEGIN_TASK2);
	        }
    	vTaskDelay( pdMS_TO_TICKS(1000) );
    	ESP_LOGI(TAG1, "2+ Espere 1 segundo");
    	}
    }
}




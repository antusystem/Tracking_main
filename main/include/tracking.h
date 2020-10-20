/*
 * tracking.h
 *
 *  Created on: Apr 7, 2020
 *      Author: jose
 *
 *
 */

#ifndef MAIN_INCLUDE_TRACKING_H_
#define MAIN_INCLUDE_TRACKING_H_

#include <AM2301.h>
#include "NMEA_setting.h"

/*Defino el tamano del buffer de recepcion de datos GPS con el modulo A9G
 * los buses llegan cada segundo y tienden a ser un string de aproximadament
 *  282 caracteres*/
#define BUF_SIZE (1024)

/*Para definir los tiempos de espera para los comandos AT*/
//#define PRODUCCION
#define TESIS

typedef enum{
	P_cerrada = 0,
	P_abierta,
} e_Puerta;

typedef enum{
	Dato_temp = 0,
	Dato_gps,
	Dato_puerta,
} e_temp_gps;

typedef struct {
    char Latitude[319];
    char Longitude[319];
    char Latitude_dir[20];
    char Longitude_dir[20];
    char Humedad[319];
    char Temperatura[319];
} message_data_t;


typedef enum{
	CMGF = 0,
	CPAS,
	CPOWD,
} e_ATCOM;

typedef enum{
	CFUN = 0,
	CSTT,
	CIICR,
	CGREG,
	CIFSR,
	CPOWD2,
} e_ATCOM2;

typedef enum{
	CIPSTART = 0,
	CIPSTART2,
	CIPSEND,
	CIPSEND2,
	CPOWD3,
} e_ATCOM3;



#ifdef TESIS
typedef enum {
    t_CFUN = 12000,
    t_CSST = 30000,
    t_CIICR = 35000,
    t_CGREG = 5000,
    t_CMGF = 12000,
    t_CIFSR = 5000,
    t_CPAS = 5000,
    t_CMGS = 31000,
	t_CIPSTART = 32000,
	t_CIPSEND = 33000,
    t_CPOWD = 5000,
} e_TEspera;
#endif

#ifdef PRODUCCION
typedef enum {
    t_CFUN = 12000,
    t_CSST = 30000,
    t_CIICR = 130000,
    t_CGREG = 5000,
    t_CMGF = 12000,
    t_CIFSR = 5000,
    t_CPAS = 5000,
    t_CMGS = 60000,
	t_CIPSTART = 162000,
	t_CIPSEND = 647000,
    t_CPOWD = 5000,
} e_TEspera;
#endif

struct TRAMA{
	uint8_t dato[BUF_SIZE];
	uint16_t size;
};


/* Tareas Asociadas*/
void uart1_event_task(void *pvParameters);
void echo_U1toU0(void *pvParameters);
void Mandar_mensaje(void *P);

/*Funciones Asociadas*/

void  Configurar_UARTs();
void  Tiempo_Espera(char* aux, uint8_t estado, uint16_t* tamano, uint8_t* error, portTickType tiempo);
void  Prender_SIM800l();
void  Envio_mensaje(char* mensaje, uint8_t tamano);
e_ATCOM  Configurar_GSM(e_ATCOM ATCOM);
e_ATCOM2  Configurar_GPRS(e_ATCOM2 ATCOM);
void Envio_GPRS(uint8_t gps_temp, uint8_t campo,float dato);
void Enviar_GPRS(gps_data_t* gps_data2, AM2301_data_t* Thum2);
void Enviar_Mensaje(gps_data_t* gps_data2, AM2301_data_t* Thum2);
void set_form_flash_init( message_data_t *datos);

#endif /* MAIN_INCLUDE_TRACKING_H_ */

/*
 * tracking.h
 *
 *  Created on: Apr 7, 2020
 *      Author: jose
 */

#ifndef MAIN_INCLUDE_TRACKING_H_
#define MAIN_INCLUDE_TRACKING_H_

typedef enum
{
	P_cerrada = 0,
	P_abierta,
} e_Puerta;


typedef struct {
    char Latitude[319];
    char Longitude[319];
    char Latitude_dir[20];
    char Longitude_dir[20];/*!< Longitude (degrees) */
    char Humedad[319];
    char Temperatura[319];

} message_data_t;


typedef enum
{
	CMGF = 0,
	CPAS,
	CPOWD,
} e_ATCOM;

typedef enum
{
	CFUN = 0,
	CSTT,
	CIICR,
	CGREG,
	CIFSR,
	CPOWD2,
} e_ATCOM2;

typedef enum
{
	CIPSTART = 0,
	CIPSTART2,
	CIPSEND,
	CIPSEND2,
	CPOWD3,
} e_ATCOM3;


//Enum para asignar los tiempos de espera para cada comando AT
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




#endif /* MAIN_INCLUDE_TRACKING_H_ */

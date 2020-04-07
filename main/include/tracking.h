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
    char Latitude_dir[10];
    char Longitude_dir[10];/*!< Longitude (degrees) */
    char Humedad[319];
    char Temperatura[319];

} message_data_t;



#endif /* MAIN_INCLUDE_TRACKING_H_ */

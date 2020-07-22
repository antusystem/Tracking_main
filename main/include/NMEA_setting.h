/*
 * NMEA_setting.h
 *
 *  Created on: Feb 27, 2020
 *      Author: jose
 */

#ifndef MAIN_NMEA_SETTING_H_
#define MAIN_NMEA_SETTING_H_
#include "esp_log.h"

/*No se usa generalmente porque esta comentado en la funcion RMC_parsing,
 * no lo borro porque puede ser necesario para debug*/
static const char *TAG2 = "GPS_Parsing";


typedef struct {
    char NMEA_GNGGA[256];
    char NMEA_GPGSA[256];
    char NMEA_BDGSA[256];
    char NMEA_GPGSV1[256];
    char NMEA_BDGSV[256];
    char NMEA_GNRMC[256];
    char NMEA_GNVTG[256];
} NMEA_data_t;

typedef enum{
	time_utc = 0,	//Valor para indicara el tiempo en UTC
	Nactive,			//Dice si esta conectado, aunque no siempre funciona bien en el A9G
	Nlatitude,
	Nlatitude_dir,
	Nlongitude,
	Nlongitude_dir,
	Nspeed,
	Ntrack,
	Ndate,
} rmc_stages;

typedef enum{
	GNGGA = 0,
	GPGSA,
	BDGSA,
	GPGSV,
	BDGSV,
	GNRMC,
	GNVTG,
} NMEA_sentences;

typedef struct {
    float latitude[41];                                                /*!< Latitude (degrees) */
    float latitude_prom;
    char latitude_direct[20];
    float longitude[41];                                               /*!< Longitude (degrees) */
    float longitude_prom;
    char longitude_direct[20];
    float altitude;                                                /*!< Altitude (meters) */
    char estado[2];
    uint8_t fix;                                                 /*!< Fix status */
    uint8_t sats_in_use;
    int hour;
    uint8_t minute;
    uint8_t second;
    uint8_t day;
    uint8_t month;
    char mes[20];
    uint16_t year;
    float dop_h;                                                   /*!< Horizontal dilution of precision */
    float dop_p;                                                   /*!< Position dilution of precision  */
    float dop_v;                                                   /*!< Vertical dilution of precision  */
    uint8_t sats_in_view;                                          /*!< Number of satellites in view */
    float speed;                                                   /*!< Ground speed, unit: m/s */
    float cog;                                                     /*!< Course over ground */
    float variation;                                               /*!< Magnetic variation */
    uint8_t error_gps;
    uint8_t ronda_error;
    uint16_t ronda;
    rmc_stages stage;
} gps_data_t;



typedef struct {
    char time_UTC[11];
    char active[2];
    char latitude[10];                                                /*!< Latitude (degrees) */
    char latitude_deg[3];
    float latitude_deg_f;
    char latitude_min[8];
    float latitude_min_f;
    char latitude_dir[2];
    char longitude[10];                                               /*!< Longitude (degrees) */
    char longitude_deg[318];
    float longitude_deg_f;
    char longitude_min[8];
    float longitude_min_f;
    char longitude_dir[2];
    char speed[6];
    char track[5];
    char date[7];   										 /*!< Number of satellites in use */
} rmc_data_t;

gps_data_t gps_data;

/* Tareas Asociadas*/
void GNSS_task(void *P);

/*Funciones Asociadas*/
void Guardar_dolar(char* auxc2_echo, uint16_t* posicion_echo);
NMEA_data_t Dividir_oraciones(NMEA_data_t NMEA_data, char* buf, uint16_t* posicion_echo);
gps_data_t RMC_parsing(char* GNRMC_data, gps_data_t *GPS_data);
gps_data_t GPS_parsing(char* data, gps_data_t GPS_data);


#endif /* MAIN_NMEA_SETTING_H_ */

/*
 * NMEA_setting.h
 *
 *  Created on: Feb 27, 2020
 *      Author: jose
 */

#ifndef MAIN_NMEA_SETTING_H_
#define MAIN_NMEA_SETTING_H_
#include "esp_log.h"

static const char *TAG2 = "GPS_Parsing";


typedef struct {
    char NMEA_GNGGA[BUF_SIZE];
    char NMEA_GPGSA[BUF_SIZE];
    char NMEA_BDGSA[BUF_SIZE];
    char NMEA_GPGSV1[BUF_SIZE];
    char NMEA_GPGSV2[BUF_SIZE];
    char NMEA_GPGSV3[BUF_SIZE];
    char NMEA_GPGSV4[BUF_SIZE];
    char NMEA_GPGSV5[BUF_SIZE];
    char NMEA_GPGSV6[BUF_SIZE];
    char NMEA_GPGSV7[BUF_SIZE];
    char NMEA_GPGSV8[BUF_SIZE];
    char NMEA_GPGSV9[BUF_SIZE];
    char NMEA_BDGSV[BUF_SIZE];
    char NMEA_GNRMC[BUF_SIZE];
    char NMEA_GNVTG[BUF_SIZE];
} NMEA_data_t;

typedef struct {
    float latitude[20];                                                /*!< Latitude (degrees) */
    char latitude_s[20];
    float latitude_prom;
    char latitude_direct[20];
    float longitude[10];                                               /*!< Longitude (degrees) */
    char longitude_s[20];
    float longitude_prom;
    char longitude_direct[20];
    float altitude;                                                /*!< Altitude (meters) */
    char estado[2];
    uint8_t fix;                                                 /*!< Fix status */
    uint8_t sats_in_use;                                           /*!< Number of satellites in use */
    uint8_t hour;      /*!< Hour */
    uint8_t minute;    /*!< Minute */
    uint8_t second;    /*!< Second */
    uint8_t day;   /*!< Day (start from 1) */
    uint8_t month; /*!< Month (start from 1) */
    char mes[20];
    uint16_t year; /*!< Year (start from 2000) */
    float dop_h;                                                   /*!< Horizontal dilution of precision */
    float dop_p;                                                   /*!< Position dilution of precision  */
    float dop_v;                                                   /*!< Vertical dilution of precision  */
    uint8_t sats_in_view;                                          /*!< Number of satellites in view */
    float speed;                                                   /*!< Ground speed, unit: m/s */
    float cog;                                                     /*!< Course over ground */
    float variation;                                               /*!< Magnetic variation */
} gps_data_t;

typedef struct {
    char time_UTC[11];
    char latitude[10];                                                /*!< Latitude (degrees) */
    char latitude_deg[318];
    float latitude_deg_f;
    char latitude_min[9];
    float latitude_min_f;
    char latitude_dir[2];
    char longitude[10];                                               /*!< Longitude (degrees) */
    char longitude_deg[318];
    float longitude_deg_f;
    char longitude_min[7];
    float longitude_min_f;
    char longitude_dir[2];
    char fix[2]; 													 /*!< Fix status */
    char sats_in_use[3];   										 /*!< Number of satellites in use */
    char dop_h[5];                                                   /*!< Horizontal dilution of precision */
    char altitude[5];                                                /*!< Altitude (meters) */
} gga_data_t;

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
gps_data_t gps_data2;

void echo_task(void *P);







#endif /* MAIN_NMEA_SETTING_H_ */

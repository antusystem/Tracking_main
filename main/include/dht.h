#ifndef __DHT_H__
#define __DHT_H__
#include "stdint.h"
/*
struct form_home{
	char Humedad1[6];
	char Temperatura1[6];
	uint16_t Humedad2;
	uint16_t Temperatura2;
	char Datos_Sensor[60];
	float Humedad3[16];
	float Temperatura3[16];
	float Prom_hum[256];
	float Prom_temp[256];
	uint8_t vuelta_error;
	uint8_t error_temp;
	uint8_t pos_temp;
};*/

typedef struct {
	char Humedad1[6];
	char Temperatura1[6];
	uint16_t Humedad2;
	uint16_t Temperatura2;
	char Datos_Sensor[60];
	float Humedad3[16];
	float Temperatura3[16];
	float Prom_hum[256];
	float Prom_temp[256];
	uint8_t vuelta_error;
	uint8_t error_temp;
	uint8_t pos_temp;
} AM2301_data_t;


AM2301_data_t Thum;


void TareaDHT(void *P);

#endif

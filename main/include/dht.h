#ifndef __DHT_H__
#define __DHT_H__
#include "stdint.h"

struct form_home{
	char Humedad1[6];
	char Temperatura1[6];
	uint16_t Humedad2;
	uint16_t Temperatura2;
	char Datos_Sensor[60];
	float Humedad3;
	float Temperatura3;
	float Prom_hum;
	float Prom_temp;
};


struct form_home form1;


void TareaDHT(void *P);

#endif

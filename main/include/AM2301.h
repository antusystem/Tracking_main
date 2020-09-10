
#ifndef __DHT_H__
#define __DHT_H__
#include "stdint.h"
#include "driver/gpio.h"

// Definiciones
#define us_retardo (1)
#define numDHT_bits (40)
#define numDHT_bytes (5)
#define DHTpin (GPIO_NUM_19)
#define Temp_up (29)
#define Temp_down (26)

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
	uint8_t primer_ciclo;
} AM2301_data_t;


AM2301_data_t Thum;

/* Tareas Asociadas*/
void TareaAM2301(void *P);

/*Funciones Asociadas*/
esp_err_t TiempoDeEspera(gpio_num_t pin, uint32_t timeout, int valor_esperado, uint32_t *contador_us);
esp_err_t CapturarDatos(gpio_num_t pin, uint8_t datos[numDHT_bytes]);
esp_err_t leerDHT(gpio_num_t pin,uint8_t *humedad, uint8_t *decimal_hum, uint8_t *temperatura, uint8_t *decimal_temp, uint8_t *signo_temp);

#endif

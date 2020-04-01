#include "dht.h"
// Inclusion de librerias -------------------------------------------
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "freertos/queue.h"
#include <string.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "freertos/semphr.h"

// Definiciones -----------------------------------------------------
#define us_retardo 1
#define numDHT_bits 40
#define numDHT_bytes 5
#define DHTpin 19


extern EventGroupHandle_t event_group;

extern QueueHandle_t xQueue_temp;

extern const int BEGIN_TASK1;

extern const int BEGIN_TASK2;

extern const int SYNC_BIT_TASK1;
/*
extern const int BEGIN_TASK2;

extern const int BEGIN_TASK3;*/



const char *nvs_tag = "NVS";
struct form_home *form2;

//Escribir en la memoria flash
void set_form_flash_init( AM2301_data_t Thum){
	esp_err_t err;
	nvs_handle_t ctrl_flash;
	err = nvs_open("storage",NVS_READWRITE,&ctrl_flash);
	if (err != ESP_OK) {
		printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
	}else{
		nvs_set_str(ctrl_flash,"Humedad1",Thum.Humedad1);

		nvs_set_str(ctrl_flash,"Temperatura1",Thum.Temperatura1);

		ESP_LOGI("NVS_FLASH","Temperatura2 a guardar en flash: %d",Thum.Temperatura2);
		nvs_set_u16(ctrl_flash,"Temp2",Thum.Temperatura2);

		nvs_set_u16(ctrl_flash,"Humedad2",Thum.Humedad2);

		ESP_LOGI("NVS_FLASH","Datos_Sensor a guardar en flash: %s",Thum.Datos_Sensor);

		nvs_set_str(ctrl_flash,"Datos_Sensor",Thum.Datos_Sensor);

		err = nvs_commit(ctrl_flash);
	}
	nvs_close(ctrl_flash);

}

//Leer la memoria flash
void get_form_flash( AM2301_data_t *Thum){
	size_t len;
	esp_err_t err;
	nvs_handle_t ctrl_flash;


	err = nvs_open("storage",NVS_READWRITE,&ctrl_flash);
	if (err != ESP_OK) {
		printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
	}else{

		err = nvs_get_str(ctrl_flash,"Humedad1",NULL,&len);
		if(err==ESP_OK) {
			err = nvs_get_str(ctrl_flash,"Humedad1",Thum->Humedad1,&len);
			switch(err){
				case ESP_OK:
					ESP_LOGI(nvs_tag,"Humedad1 en flash: %s",Thum->Humedad1);
				break;
				case ESP_ERR_NVS_NOT_FOUND:
					ESP_LOGI(nvs_tag,"Humedad1 en flash: none");
				break;
				default:
					printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
				break;
			}
		}

		err = nvs_get_str(ctrl_flash,"Temperatura1",NULL,&len);
		if(err==ESP_OK){
			err= nvs_get_str(ctrl_flash,"Temperatura1",Thum->Temperatura1,&len);
		switch(err){
			case ESP_OK:
				ESP_LOGI(nvs_tag,"Temperatura1 en flash: %s",Thum->Temperatura1);
			break;
			case ESP_ERR_NVS_NOT_FOUND:
				ESP_LOGI(nvs_tag,"Temperatura1 en flash: none");
			break;
			default:
				printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
			break;

		}
		err = nvs_get_u16(ctrl_flash,"Humedad2",&(Thum->Humedad2));
							switch(err){
								case ESP_OK:
									ESP_LOGI(nvs_tag,"Humedad2 en flash: %d",Thum->Humedad2);
								break;
								case ESP_ERR_NVS_NOT_FOUND:
									ESP_LOGI(nvs_tag,"Humedad2 en flash: none");
								break;
								default:
									printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
								break;
							}
		}
	}nvs_close(ctrl_flash);

	err = nvs_open("storage",NVS_READWRITE,&ctrl_flash);

		if (err != ESP_OK) {
			printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
		}else{
			err = nvs_get_u16(ctrl_flash,"Temp2",&(Thum->Temperatura2));
								switch(err){
									case ESP_OK:
										ESP_LOGI(nvs_tag,"Temperatura2 en flash: %d",Thum->Temperatura2);
									break;
									case ESP_ERR_NVS_NOT_FOUND:
										ESP_LOGI(nvs_tag,"Temperatura2 en flash: none");
									break;
									default:
										printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
									break;
								}

			err = nvs_get_str(ctrl_flash,"Datos_Sensor",NULL,&len);
			if(err==ESP_OK) {
				err = nvs_get_str(ctrl_flash,"Datos_Sensor",Thum->Datos_Sensor,&len);
				switch(err){
					case ESP_OK:
						ESP_LOGI(nvs_tag,"Datos_Sensor en flash: %s",Thum->Datos_Sensor);
					break;
					case ESP_ERR_NVS_NOT_FOUND:
						ESP_LOGI(nvs_tag,"Datos_Sensor en flash: none");
					break;
					default:
						printf("Error (%s) opening NVS handle!\n", esp_err_to_name(err));
					break;
				}
			}
	}
	nvs_close(ctrl_flash);
}




static portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;
// Funcion para medir tiempo en us ----------------------------------
static esp_err_t TiempoDeEspera(gpio_num_t pin,
		                        uint32_t timeout,
								int valor_esperado,
								uint32_t *contador_us){
    gpio_set_direction(pin, GPIO_MODE_INPUT);
    for (uint32_t i = 0; i < timeout; i += us_retardo){
        ets_delay_us(us_retardo);
        if(gpio_get_level(pin) == valor_esperado){
            if(contador_us) *contador_us = i;
            return ESP_OK;
        }
    }
    return ESP_ERR_TIMEOUT;
}
// Funcion para capturar bits del DHT11 -----------------------------
static esp_err_t CapturarDatos(gpio_num_t pin, uint8_t datos[numDHT_bytes]){
    uint32_t tiempo_low;
    uint32_t tiempo_high;
    // Inicio de la secuencia de lectura
    gpio_set_direction(pin, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(pin, 0);
    ets_delay_us(1000);
    gpio_set_level(pin, 1);
    // Se espera hasta 220us por un flanco de bajada desde el DHT
    if(TiempoDeEspera(pin, 220, 0, NULL)!=ESP_OK)return ESP_ERR_TIMEOUT;
    // Se espera hasta 90us por un flanco de subida
    if(TiempoDeEspera(pin, 90, 1, NULL)!=ESP_OK)return ESP_ERR_TIMEOUT;
    // Se espera hasta 90us por un flanco de bajada
    if(TiempoDeEspera(pin, 90, 0, NULL)!=ESP_OK)return ESP_ERR_TIMEOUT;
    // Si la respuesta fue satisfactoria, se comienzan a recibir los datos
    for (int i = 0; i < numDHT_bits; i++){
    	if(TiempoDeEspera(pin, 60, 1, &tiempo_low)!=ESP_OK)return ESP_ERR_TIMEOUT;
    	if(TiempoDeEspera(pin, 80, 0, &tiempo_high)!=ESP_OK)return ESP_ERR_TIMEOUT;
        uint8_t b = i / 8, m = i % 8;
        if (!m)datos[b] = 0;
        datos[b] |= (tiempo_high > tiempo_low) << (7 - m);
    }
    return ESP_OK;
}

static esp_err_t leerDHT(gpio_num_t pin,uint8_t *humedad, uint8_t *decimal_hum, uint8_t *temperatura, uint8_t *decimal_temp, uint8_t *signo_temp){
    uint8_t datos[numDHT_bytes] = {0,0,0,0,0};
    gpio_set_direction(pin, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(pin, 1);

    portENTER_CRITICAL(&mux);
    esp_err_t resultado = CapturarDatos(pin, datos);
    portEXIT_CRITICAL(&mux);

    gpio_set_direction(pin, GPIO_MODE_OUTPUT_OD);
    gpio_set_level(pin, 1);
    if(resultado != ESP_OK)return resultado;
    if(datos[4] != ((datos[0] + datos[1] + datos[2] + datos[3]) & 0xFF)){
        ESP_LOGE("Sensor_AM2301","Error en verificacion de Checksum");
        return ESP_ERR_INVALID_CRC;
    }
    *humedad = datos[0];
    *decimal_hum = datos[1];
    *signo_temp  = (datos[2]&0x80) >> 7;
    *temperatura  = datos[2]&0x7F;
    *decimal_temp = datos[3];


    return ESP_OK;
}

void TareaDHT(void *P){

	printf("Entre en TareaDHT \r\n");
	uint8_t temperatura = 0, decimal_temp = 0, signo_temp = 0;
    uint8_t humedad = 0, decimal_hum = 0, sirve = 0, vuelta_temp = 0;
    uint16_t auxi1 = 0, auxi2 = 0;
    char auxc1[54] = "", auxc2[54] = "";
    int auxi3 = 0, auxi4 = 0;
    float prom_temp = 0, prom_hum = 0;

    //Para saber la posicion en que estoy dentro del arreglo de promedios de temp y hum
    Thum.pos_temp = 0;

    //vuelta error indica cuantas vueltas a dado con error, si llega a 10 salta al GPS
    // y con error temp se verifica cuando se envia el mensaje si se logro medir la temperatura

    char datos_sensor[]={"-Humedad Relativa = 00.0%\n\r-Temperatura = +00.0 C\n\n\r"};

    for(;;){

    	xEventGroupWaitBits(event_group,BEGIN_TASK1,pdFALSE,true,portMAX_DELAY);

    	//Prendo el led 2 veces en un segundo para saber en que etapa entre
    	sirve = 0;
    	for (int j1 = 0; j1 < 16; j1++ ){
    //	ESP_LOGI("PRUEBA","Esperare 3 segundos");
    	vTaskDelay(3000 / portTICK_PERIOD_MS);
        if (leerDHT(DHTpin, &humedad, &decimal_hum, &temperatura, &decimal_temp, &signo_temp) == ESP_OK){
        	//Determinar el signo de la temperatura
        	if (signo_temp == 1){
        		datos_sensor[42]= '-';
        	}

        //	ESP_LOGI("Sensor_AM2301","Humedad %d.%d %% Temperatura: +%d.%dC",humedad,decimal_hum,temperatura,decimal_temp);

        	//Determinar la humedad diviendo la llegada de los bit mas significativos
        	switch (humedad){
        	case 0:
				auxi1 = 0;
        	break;
        	case 1:
				auxi1 = 256;
        	break;
        	case 2:
				auxi1 = 512;
        	break;
        	}

        	//Determinar la temperatura diviendo la llegada de los bit mas significativos
        	switch (temperatura){
        	case 0:
				auxi2 = 0;
        	break;
        	case 1:
				auxi2 = 256;
        	break;
        	}


//        	ESP_LOGI("Sensor_AM2301","%s", datos_sensor);
        	//Guardar el valor de la humedad
        	auxi3 = (auxi1 + decimal_hum);
        	//Poner en un string el valor de la humedad
//        	ESP_LOGI("Sensor_AM2301","auxi3: %d", auxi3);
        	sprintf(auxc1,"%d",auxi3);
//        	ESP_LOGI("Sensor_AM2301","auxc1: %s", auxc1);

        	//Guardar el valor de la temperatura
        	auxi4 = (auxi2 + decimal_temp);
 //       	ESP_LOGI("Sensor_AM2301","auxi4: %d", auxi4);
        	//Poner en un string el valor de la temperatura
        	sprintf(auxc2,"%d",auxi4);
 //       	ESP_LOGI("Sensor_AM2301","auxc2: %s", auxc2);

        	datos_sensor[20] = auxc1[0];
        	datos_sensor[21] = auxc1[1];
        	datos_sensor[23] = auxc1[2];
 //       	ESP_LOGI("Sensor_AM2301","auxc1: %c", auxc1[0]);

        	datos_sensor[43] = auxc2[0];
            datos_sensor[44] = auxc2[1];
        	datos_sensor[46] = auxc2[2];

 //       	ESP_LOGI("Sensor_AM2301","Guardare los datos en el struct");
         	sprintf(Thum.Humedad1,"%d",auxi3);
            sprintf(Thum.Temperatura1,"%d",auxi4);
            Thum.Humedad2 = auxi3;
            Thum.Temperatura2 = auxi4;
            sprintf(Thum.Datos_Sensor,"%s",datos_sensor);
            Thum.Humedad3[j1] = (float) auxi3/10;
            Thum.Temperatura3[j1] = (float) auxi4/10;
            ESP_LOGI("Sensor_AM2301","La Humedad es: %.1f %% \r\n La temperatura es: %.1f C \r\n", Thum.Humedad3[j1],Thum.Temperatura3[j1]);

            sirve = 0;
            Thum.vuelta_error = 0;

            vuelta_temp++;
            ESP_LOGI("Sensor_AM2301","La vuelta es: %d", vuelta_temp);

            for (int i = 0; i < 16; i++ ){
            	prom_hum += Thum.Humedad3[i];
            	prom_temp += Thum.Temperatura3[i];
            }
       //     ESP_LOGI("Sensor_AM2301","Prom hum es: %f", prom_hum);
       //     ESP_LOGI("Sensor_AM2301","Prom temp es: %f", prom_temp);
            Thum.Prom_hum[Thum.pos_temp] = prom_hum/16;
            Thum.Prom_temp[Thum.pos_temp] = prom_temp/16;
            prom_hum =  0.0;
            prom_temp = 0.0;
        	ESP_LOGI("PRUEBA","La humedad promedio es %.1f",Thum.Prom_hum[Thum.pos_temp]);
        	ESP_LOGI("PRUEBA","La temperatura promedio es %.1f",Thum.Prom_temp[Thum.pos_temp]);
        	Thum.pos_temp++;
        	xQueueOverwrite(xQueue_temp,&Thum);
        }
        else{
            ESP_LOGE("Sensor_AM2301","No fue posible leer datos del AM2301");
            sirve = 1;
            Thum.vuelta_error++;
            //Aqui verifico si ya tuve 10 errores seguidos midiendo la temperatura,
            //al 10mo sigue a la siguiente tarea y deja el flag error temp en 1
            if (Thum.vuelta_error >= 10){
            	Thum.vuelta_error = 0;
            	Thum.error_temp = 1;
            	xEventGroupSetBits(event_group, SYNC_BIT_TASK1);
            	break;
            }
        }

        if (sirve == 0 && vuelta_temp >=8){
        	vuelta_temp = 0;
        	xEventGroupSetBits(event_group, SYNC_BIT_TASK1);
        	xEventGroupSetBits(event_group, BEGIN_TASK2);
        	xEventGroupClearBits(event_group, BEGIN_TASK1);
    		break;
        }
        }
    }
}

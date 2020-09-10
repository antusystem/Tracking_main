# Tracking_main 


## Overview


An application that integrates the ESP32, the SIM800L, the temperature-humidity sensor AM2301, and the dev board A9G. This code use the ESP32 has the main microcontroler of the group, and it is in charge of asking the temperature to the sensor and process it, giving the AT Commands to the A9G to start the GPS and to start the to deliver the data, and also to give turn on the SIM800L and then give it the AT Commands for it to forward the data acquired from the GPS and the sensor to a mobile cellphone (by 2G) and to thingspeak (by GPRS with through HTTP GET). It also check the state a pin connected to the door sending information if it is open at anytime.

The AT Commands have their parsing. The GPS raw data is also parse before sending it


## How to use example

### Hardware Required

For this it was used the TTGO T-Call SIM800L that includes an ESP32 and a SIM800L in the same dev board, the temperature-humidity sensor AM2301, and the dev board A9G from Ai Thinker that has GPS and GPRS (though the GPRS was not used with the A9G).

Remember that **SIM800L only suports 2G and GPRS and it might not work in some countries.**

#### Pin Assignment

SIM800l_PWR_KEY: SIM800l Power Key pin
SIM800l_PWR: SIM800L Power pin
SIM800l_RST: SIM800L Reset pin

|       ESP32     |     SIM800L    |
| ------ -------- | -------------- |
|      GPIO27     |       TX       |
|      GPIO26     |       RX       |
|      GPIO04     | SIM800l_PWR_KEY|
|      GPIO23     |   SIM800l_PWR  |
|      GPIO05     |   SIM800l_RST  |

|       ESP32     |      AM2301    |
| --------------- | -------------- |
|      GPIO19     |    Data line   |
|       3,3 V     |       VCC      |
|        GND      |       GND      |

|       ESP32     |       A9G      |
| --------------- | -------------- |
|      GPIO18     |       TX       |
|      GPIO00     |       RX       |
|        GND      |       GND      |



### Configure the project

To configure the pins you have to change them manually in:

- For the SIM800L communication go to uart.c
- For the SIM800L power up go to GSM.c
- For the A9G communication go to uart.c
- For the AM2301 data line pin go to AM2301.h
- For the phone number that will recieve the message go to GSM.c in the define Phone_Number
- For the API key of thingspeak go to GSM.c in the define API_KEY


## Example Output

The example will get module and operator's information after start up, and then go into PPP mode to start mqtt client operations. This example will also send a short message to someone's phone if you have enabled this feature in menuconfig.

```

## Troubleshooting

Check de IDFPATH in the configurations of the proyect

## Log

Last compile: September 10th, 2020.
Last test: July 15, 2020
Last compile espidf version: v4.3-dev-472-gcf056a7d0

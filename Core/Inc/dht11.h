#ifndef __DHT11_H
#define __DHT11_H

#include "stm32f1xx_hal.h"

#define DHT11_PORT    GPIOB
#define DHT11_PIN     GPIO_PIN_8

typedef struct {
    float temperature;
    float humidity;
} DHT11_Data;

void DHT11_Init(void);
uint8_t DHT11_Read(DHT11_Data *data);

#endif /* __DHT11_H */
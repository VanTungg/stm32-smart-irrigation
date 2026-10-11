#ifndef __SOIL_SENSOR_H__
#define __SOIL_SENSOR_H__

#include "main.h"

typedef struct {
    uint16_t adc;     // Giá trị thanh ghi ADC (0 - 4095)
    float    voltage; // Điện áp quy đổi (0.0V - 3.3V)
} SoilSensor;

void Soil_Read(ADC_HandleTypeDef *hadc, SoilSensor *s);

#endif
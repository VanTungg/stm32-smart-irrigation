#ifndef __SOIL_SENSOR_H__
#define __SOIL_SENSOR_H__

#include "main.h"

typedef struct {
    uint16_t dry; // Mốc khô (ADC = 4000)
    uint16_t wet; // Mốc ướt (ADC = 1300)
    uint16_t adc; // ADC lọc 50 mẫu
    uint16_t  pct; // Độ ẩm % (0 - 100%)
} SoilSensor;

void Soil_Init(SoilSensor *s, uint16_t dry, uint16_t wet);
void Soil_Read(ADC_HandleTypeDef *hadc, SoilSensor *s);

#endif
#ifndef SOIL_SENSOR_H
#define SOIL_SENSOR_H

#include "main.h"

#define ADC_DRY 3300
#define ADC_WET 1400

void SoilSensor_Read(ADC_HandleTypeDef *hadc,
                     uint16_t *adc_raw,
                     uint16_t *adc_filtered,
                     float *voltage,
                     uint8_t *moisture);

#endif
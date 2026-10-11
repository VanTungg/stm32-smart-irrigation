#include "soil_sensor.h"

void Soil_Read(ADC_HandleTypeDef *hadc, SoilSensor *s) {
    HAL_ADC_Start(hadc);
    if (HAL_ADC_PollForConversion(hadc, 2) == HAL_OK) {
        s->adc = (uint16_t)HAL_ADC_GetValue(hadc);
        s->voltage = (float)s->adc * 3.3f / 4095.0f;
    }
    HAL_ADC_Stop(hadc);
}
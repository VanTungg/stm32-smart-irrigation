#include "soil_sensor.h"

void Soil_Init(SoilSensor *s, uint16_t dry, uint16_t wet) {
    s->dry = dry;
    s->wet = wet;
    s->adc = 0;
    s->pct = 0;
}

void Soil_Read(ADC_HandleTypeDef *hadc, SoilSensor *s) {
    uint32_t sum = 0;

    for (uint8_t i = 0; i < 50; i++) {
        HAL_ADC_Start(hadc);
        if (HAL_ADC_PollForConversion(hadc, 2) == HAL_OK) {
            sum += HAL_ADC_GetValue(hadc);
        }
        HAL_ADC_Stop(hadc);
    }
    s->adc = (uint16_t)(sum / 50);

    int32_t val = 0;
    if (s->dry > s->wet) {
        val = (int32_t)(s->dry - s->adc) * 100 / (s->dry - s->wet);
    }
    if (val < 0) val = 0;
    if (val > 100) val = 100;

    s->pct = (uint8_t)val;
}
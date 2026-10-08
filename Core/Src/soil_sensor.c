#include "soil_sensor.h"

void SoilSensor_Read(ADC_HandleTypeDef *hadc,
                     uint16_t *adc_raw,
                     uint16_t *adc_filtered,
                     float *voltage,
                     uint8_t *moisture)
{
    uint32_t adc_sum = 0;
    int32_t moisture_temp;

    for (int i = 0; i < 10; i++)
    {
        HAL_ADC_Start(hadc);

        if (HAL_ADC_PollForConversion(hadc, 100) == HAL_OK)
        {
            *adc_raw = HAL_ADC_GetValue(hadc);
            adc_sum += *adc_raw;
        }

        HAL_ADC_Stop(hadc);
    }

    *adc_filtered = adc_sum / 10;

    *voltage = ((float)(*adc_filtered) * 3.3f) / 4095.0f;

    moisture_temp = ((int32_t)ADC_DRY - (int32_t)(*adc_filtered)) * 100
                    / ((int32_t)ADC_DRY - (int32_t)ADC_WET);

    if (moisture_temp < 0)
    {
        moisture_temp = 0;
    }

    if (moisture_temp > 100)
    {
        moisture_temp = 100;
    }

    *moisture = (uint8_t)moisture_temp;
}
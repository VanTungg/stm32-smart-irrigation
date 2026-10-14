#ifndef __SOIL_SENSOR_H__
#define __SOIL_SENSOR_H__

#include "main.h"

// Trạng thái cảm biến
typedef enum {
    SOIL_OK = 0,
    SOIL_ERROR
} SoilStatus;

typedef struct {
    uint32_t dry;            // Mốc khô
    uint32_t wet;            // Mốc ướt
    uint32_t adc_raw;        // ADC thô sau khi lọc
    float adc_voltage;       // Điện áp
    float soil_moisture;     // % độ ẩm
    SoilStatus status;       // Trạng thái báo lỗi
} SoilSensor;

void Soil_Init(SoilSensor *s, uint32_t dry, uint32_t wet);
void Soil_Read(ADC_HandleTypeDef *hadc, SoilSensor *s);

#endif
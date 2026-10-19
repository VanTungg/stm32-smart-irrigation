#include "soil_sensor.h"

void Soil_Init(SoilSensor *s, uint32_t dry, uint32_t wet) {
    s->dry = dry;
    s->wet = wet;
    s->adc_raw = 0;
    s->adc_voltage = 0.0f;
    s->soil_moisture = 0.0f;
    s->status = SOIL_OK;
}

void Soil_Read(ADC_HandleTypeDef *hadc, SoilSensor *s) {
    uint32_t sum = 0;
    uint8_t valid_samples = 0;
    
    // Lọc trung bình 50 mẫu, có kiểm tra mã lỗi HAL
    for (uint8_t i = 0; i < 50; i++) {
        HAL_ADC_Start(hadc);
        if (HAL_ADC_PollForConversion(hadc, 10) == HAL_OK) { // Có timeout 10ms
            sum += HAL_ADC_GetValue(hadc);
            valid_samples++;
        }
        HAL_ADC_Stop(hadc);
    }

    if (valid_samples == 0) {
        s->status = SOIL_ERROR; // Lỗi phần cứng ADC
        return;
    }

    s->adc_raw = sum / valid_samples;
    s->adc_voltage = ((float)s->adc_raw * 3.3f) / 4095.0f;

    // SỬA: Chỉ báo lỗi khi ADC = 0 (chập GND) hoặc tụt dây quá mức
    if (s->adc_raw < 10) { 
        s->status = SOIL_ERROR;
        s->soil_moisture = 0.0f; 
        return;
    } else {
        s->status = SOIL_OK;
    }

    // Tính % độ ẩm (Giữ nguyên thuật toán của bạn)
    float moisture = 0.0f;
    if (s->dry > s->wet) {
        moisture = (float)(s->dry - s->adc_raw) / (float)(s->dry - s->wet) * 100.0f;
    }
    if (moisture < 0.0f) moisture = 0.0f;
    if (moisture > 100.0f) moisture = 100.0f;
    s->soil_moisture = moisture;
}
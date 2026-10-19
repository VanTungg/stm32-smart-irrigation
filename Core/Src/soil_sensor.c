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

    //  Lọc trung bình 50 mẫu 
    for (uint8_t i = 0; i < 50; i++) {
        HAL_ADC_Start(hadc);
        HAL_ADC_PollForConversion(hadc, 10); // Chờ  10ms
        sum += HAL_ADC_GetValue(hadc);
        HAL_ADC_Stop(hadc);
    }
    s->adc_raw = sum / 50;

    //  Tính điện áp 
    s->adc_voltage = ((float)s->adc_raw * 3.3f) / 4095.0f;

    //  Bắt lỗi ngắt kết nối hoặc chập mạch
    if (s->adc_raw < 100 || s->adc_raw > 4050) {
        s->status = SOIL_ERROR;
        s->soil_moisture = 0.0f; 
        return; // Bị lỗi thì dừng hàm ở đây luôn, không tính % nữa
    } else {
        s->status = SOIL_OK;
    }

    //  Tính % độ ẩm 
    float moisture = 0.0f;
    if (s->dry > s->wet) {
        // Công thức: (Mốc khô - ADC đo được) / Khoảng cách (Khô - Ướt) * 100
        moisture = (float)(s->dry - s->adc_raw) / (float)(s->dry - s->wet) * 100.0f;
    }

    //  Chặn trên/dưới để số không bị âm hoặc vượt 100%
    if (moisture < 0.0f) moisture = 0.0f;
    if (moisture > 100.0f) moisture = 100.0f;

    s->soil_moisture = moisture;
}
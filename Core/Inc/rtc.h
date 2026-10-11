#ifndef __RTC_H
#define __RTC_H
#include "main.h"
#include <stdbool.h>
// Cấu trúc lịch tưới
typedef struct {
    uint8_t hour;
    uint8_t minute;
    bool enabled;
} LichTuoi_t;

extern volatile uint8_t rtc_hour;
extern volatile uint8_t rtc_minute;
extern volatile uint8_t rtc_second;
void RTC_Init_Module(void);

void RTC_Process(void);
// Hàm cài đặt Lịch & T_max
void RTC_DatLich1(uint8_t hour, uint8_t minute, bool enable);
void RTC_DatLich2(uint8_t hour, uint8_t minute, bool enable);
void RTC_DatThoiGianMax(uint32_t seconds);
// Hàm kiểm tra logic
bool RTC_KiemTraBomDangChay(void);
bool RTC_KiemTraDenGioTuoi(void);
bool RTC_KiemTraQuaThoiGianMax(uint32_t pump_start_tick);
#endif 
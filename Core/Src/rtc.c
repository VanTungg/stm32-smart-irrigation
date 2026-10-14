#include "rtc.h"
extern RTC_HandleTypeDef hrtc;
volatile uint8_t rtc_hour = 0;
volatile uint8_t rtc_minute = 0;
volatile uint8_t rtc_second = 0;
static uint32_t last_rtc_tick = 0;
// Các biến quản lý trạng thái bơm nội bộ
static bool bom_dang_chay = false;
static uint32_t pump_start_tick = 0; // biến đếm thời gian bật bơm 
// Khai báo mặc định 2 lịch (3h46 và 3h47) & T_max = 30 giây
static LichTuoi_t lichtuoi1= {3, 46, true};   // Lịch 1: 03:46
static LichTuoi_t lichtuoi2 = {3, 47, true};  // Lịch 2: 03:47
static uint32_t thoi_gian_max_giay = 30;        // T_max = 30 giây
void RTC_Init_Module(void)
{
    RTC_TimeTypeDef sTime = {0};
    sTime.Hours = 23;
    sTime.Minutes = 59;
    sTime.Seconds = 0;
    HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    RTC_DateTypeDef sDate = {0};
    sDate.WeekDay = RTC_WEEKDAY_MONDAY;
    sDate.Month = RTC_MONTH_JANUARY;
    sDate.Date = 1;
    sDate.Year = 26;
    HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
}
// 1. Kiểm tra điểm hẹn tưới
bool RTC_KiemTraDenGioTuoi(void) {
    if (rtc_second == 0) {
        if (lichtuoi1.enabled && rtc_hour == lichtuoi1.hour && rtc_minute == lichtuoi1.minute) {
            return true;
        }
        if (lichtuoi2.enabled && rtc_hour == lichtuoi2.hour && rtc_minute == lichtuoi2.minute) {
            return true;
        }
    }
    return false;
}
// 2. Kiểm tra an toàn T_max (Bơm chạy quá 30 giây)
bool RTC_KiemTraQuaThoiGianMax(uint32_t tick_bat) {
    if (tick_bat == 0) return false;
    
    uint32_t thoi_gian_da_chay = (HAL_GetTick() - tick_bat) / 1000;
    if (thoi_gian_da_chay >= thoi_gian_max_giay) {
        return true;
    }
    return false;
}
void RTC_Process(void)
{
    if (HAL_GetTick() - last_rtc_tick >= 500)
    {
       last_rtc_tick = HAL_GetTick();
        RTC_TimeTypeDef sTime;
        RTC_DateTypeDef sDate;
        if (HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN) == HAL_OK)
        {
            HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN); 
            rtc_hour = sTime.Hours;
            rtc_minute = sTime.Minutes;
            rtc_second = sTime.Seconds;
        }
    }
    // Tự động BẬT BƠM khi đến mốc 03:46 hoặc 03:47
    if (RTC_KiemTraDenGioTuoi() && !bom_dang_chay) {
        bom_dang_chay = true;
        pump_start_tick = HAL_GetTick(); // Lưu thời điểm bật bơm!
    }
    // Tự động NGẮT BƠM sau 30 giây
    if (bom_dang_chay && RTC_KiemTraQuaThoiGianMax(pump_start_tick)) {
        bom_dang_chay = false;
    }
}  
// Cài đặt Lịch 1
void RTC_DatLich1(uint8_t hour, uint8_t minute, bool enable) {
    lichtuoi1.hour = hour;
    lichtuoi1.minute = minute;
     lichtuoi1.enabled = enable;
}

// Cài đặt Lịch 2
void RTC_DatLich2(uint8_t hour, uint8_t minute, bool enable) {
     lichtuoi2.hour = hour;
     lichtuoi2.minute = minute;
     lichtuoi2.enabled = enable;
}

// Cài đặt T_max (tính bằng giây)
void RTC_DatThoiGianMax(uint32_t seconds) {
    thoi_gian_max_giay = seconds;
}
// Hàm bổ sung cho external caller đọc trạng thái bơm
bool RTC_KiemTraBomDangChay(void) {
    return bom_dang_chay;
}

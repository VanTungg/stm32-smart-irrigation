#include "rtc.h"

extern RTC_HandleTypeDef hrtc;

// Biến thời gian gốc
volatile uint8_t rtc_hour = 0;
volatile uint8_t rtc_minute = 0;
volatile uint8_t rtc_second = 0;

static uint32_t last_rtc_tick = 0; // Tick đếm 500ms đọc RTC

// Cấu hình chu kỳ xung: BẬT 10 giây -> TẮT 5 giây
static uint32_t thoi_gian_max_giay = 10;   // Thời gian BẬT bơm mỗi nhịp (10s)
static uint32_t thoi_gian_nghi_giay = 5;    // Thời gian TẮT NGHĨ bơm (5s)

// Cấu hình ngắt an toàn: Giới hạn tối đa 1 phiên tưới (Mặc định 15 phút = 900 giây)
static uint32_t thoi_gian_toida_phien_giay = 70; 

// Quản lý trạng thái
static volatile bool phien_tuoi_dang_chay = false; // Cờ đánh dấu phiên tưới đang diễn ra
static volatile bool bom_dang_chay = false;        // Trạng thái BẬT/TẮT thực tế của bơm
static volatile uint32_t pump_start_tick = 0;      // Tick đếm chu kỳ 10s ON / 5s OFF
static volatile uint32_t tick_bat_phien = 0;       // Tick chốt mốc BẮT ĐẦU phiên tưới

// Khai báo mặc định 2 lịch (03:46 và 03:47)
static LichTuoi_t lichtuoi1 = {3, 46, true};
static LichTuoi_t lichtuoi2 = {3, 48, true};

void RTC_Init_Module(void)
{
    RTC_TimeTypeDef sTime = {0};
    sTime.Hours = 3;
    sTime.Minutes = 45;
    sTime.Seconds = 50; 
    HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);

    RTC_DateTypeDef sDate = {0};
    sDate.WeekDay = RTC_WEEKDAY_MONDAY;
    sDate.Month = RTC_MONTH_JANUARY;
    sDate.Date = 1;
    sDate.Year = 26;
    HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    phien_tuoi_dang_chay = false;
    bom_dang_chay = false;
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

// 2. Logic xử lý chính
void RTC_Process(void)
{
    uint32_t tick_hien_tai = HAL_GetTick();

    // Cập nhật giờ RTC mỗi 500ms
    if (tick_hien_tai - last_rtc_tick >= 500)
    {
        last_rtc_tick = tick_hien_tai;
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

    // Tự động KÍCH HOẠT PHIÊN TƯỚI khi đến giờ hẹn
    if (RTC_KiemTraDenGioTuoi() && !phien_tuoi_dang_chay) {
        phien_tuoi_dang_chay = true;
        bom_dang_chay = true;             // Bật bơm nhịp 10s đầu tiên
        pump_start_tick = tick_hien_tai;   // Mốc đếm chu kỳ 10s/5s
        tick_bat_phien = tick_hien_tai;    // CHỐT MỐC BẮT ĐẦU PHIÊN TƯỚI
    }

    // Quản lý phiên tưới
    if (phien_tuoi_dang_chay) {
        // KIỂM TRA BẢO VỆ 1: Quá thời gian an toàn tối đa (15 phút) -> TỰ TẮT HẲN
        if (tick_hien_tai - tick_bat_phien >= (thoi_gian_toida_phien_giay * 1000)) {
            RTC_DungTuoi(); // Tự động dập phien_tuoi_dang_chay & bom_dang_chay về false
        }
        else {
            // XỬ LÝ 2: Chu kỳ xung BẬT 10s (10000ms) -> TẮT 5s (5000ms)
            if (bom_dang_chay) {
                if (tick_hien_tai - pump_start_tick >= (thoi_gian_max_giay * 1000)) {
                    bom_dang_chay = false;
                    pump_start_tick = tick_hien_tai; // Mốc tính 5s TẮT
                }
            } 
            else {
                if (tick_hien_tai - pump_start_tick >= (thoi_gian_nghi_giay * 1000)) {
                    bom_dang_chay = true;
                    pump_start_tick = tick_hien_tai; // Mốc tính 10s BẬT
                }
            }
        }
    }
}

// Hàm DỪNG TƯỚI HẲN (Được gọi khi cảm biến đủ ẩm HOẶC bị quá thời gian an toàn)
void RTC_DungTuoi(void) {
    phien_tuoi_dang_chay = false;
    bom_dang_chay = false;
}

// Cài đặt giới hạn thời gian an toàn phiên tưới (ví dụ: 600s = 10 phút)
void RTC_DatThoiGianAnToanPhien(uint32_t giay) {
    thoi_gian_toida_phien_giay = giay;
}

// Các hàm Setter khác
void RTC_DatLich1(uint8_t hour, uint8_t minute, bool enable) {
    lichtuoi1.hour = hour;
    lichtuoi1.minute = minute;
    lichtuoi1.enabled = enable;
}

void RTC_DatLich2(uint8_t hour, uint8_t minute, bool enable) {
    lichtuoi2.hour = hour;
    lichtuoi2.minute = minute;
    lichtuoi2.enabled = enable;
}

void RTC_DatThoiGianMax(uint32_t seconds) {
    thoi_gian_max_giay = seconds;
}

void RTC_DatThoiGianNghi(uint32_t seconds) {
    thoi_gian_nghi_giay = seconds;
}

bool RTC_KiemTraBomDangChay(void) {
    return bom_dang_chay;
}

bool RTC_KiemTraPhienTuoiDangChay(void) {
    return phien_tuoi_dang_chay;
}
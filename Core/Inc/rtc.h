#ifndef __RTC_H
#define __RTC_H
#include "main.h"
#include <stdbool.h>
typedef struct {
    uint8_t hour;
    uint8_t minute;
    bool enabled;
} LichTuoi_t;

extern volatile uint8_t rtc_hour;
extern volatile uint8_t rtc_minute;
extern volatile uint8_t rtc_second;

// Khai báo API chính
void RTC_Init_Module(void);
void RTC_Process(void);

// Hàm dừng tưới (Gợi ý cho SV1/SV4 gọi khi cảm biến báo ĐỦ ĐỘ ẨM)
void RTC_DungTuoi(void);

// Các hàm cài đặt tham số
void RTC_DatLich1(uint8_t hour, uint8_t minute, bool enable);
void RTC_DatLich2(uint8_t hour, uint8_t minute, bool enable);
void RTC_DatThoiGianMax(uint32_t seconds);   // Đặt thời gian BẬT  10s
void RTC_DatThoiGianNghi(uint32_t seconds);  // Đặt thời gian TẮT  5s
void RTC_DatThoiGianAnToanPhien(uint32_t giay);
// Hàm kiểm tra trạng thái
bool RTC_KiemTraBomDangChay(void);
bool RTC_KiemTraPhienTuoiDangChay(void);
bool RTC_KiemTraDenGioTuoi(void);

#endif /* __RTC_H */
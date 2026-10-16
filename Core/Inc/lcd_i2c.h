/**
 * @file    lcd.h
 * @brief   LCD 16x02 (HD44780 + PCF8574, I2C) và giao diện 4 màn hình cho hệ thống tưới cây.
 *
 * Bốn màn hình (nút MODE chuyển vòng 1 -> 2 -> 3 -> 4 -> 1):
 *   1. LCD_SCREEN_TIME      : xem / sửa ngày giờ
 *   2. LCD_SCREEN_SENSOR    : xem thông số cảm biến (chỉ xem)
 *   3. LCD_SCREEN_THRESHOLD : cài ngưỡng, chọn AUTO/MANU, bật tắt bơm (khi MANU)
 *   4. LCD_SCREEN_SCHEDULE  : xem / sửa 2 lịch tưới
 *
 * Mô hình sử dụng (xem main.c):
 *   LCD_Init(...);                       // 1 lần
 *   while (1) {
 *       BTN_Scan(now);
 *       evt = BTN_GetEvent();
 *       LCD_HandleEvent(&lcd, evt, now); // đưa nút nhấn vào giao diện
 *       LCD_Task(&lcd, now);             // gửi dữ liệu ra LCD từng bước nhỏ
 *   }
 *
 * Toàn bộ module KHÔNG dùng HAL_Delay hay vòng lặp chờ. Mỗi lần LCD_Task() chỉ gửi
 * tối đa 1 byte lệnh/dữ liệu qua I2C (~0,5 ms ở 100 kHz) rồi trả quyền về vòng lặp chính.
 */
#ifndef LCD_I2C_H
#define LCD_I2C_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"
#include "btn.h"

/* ===================== CẤU HÌNH PHẦN CỨNG ===================== */

/* Địa chỉ I2C 7 bit của module PCF8574: thường là 0x27 (PCF8574) hoặc 0x3F (PCF8574A).
 * Hàm HAL cần địa chỉ đã DỊCH TRÁI 1 BIT -> dùng macro LCD_I2C_ADDR_HAL() để đổi,
 * tránh nhầm lẫn: toàn dự án chỉ truyền địa chỉ 7 bit vào LCD_Init().                 */
#define LCD_I2C_ADDR_7BIT       0x27U
#define LCD_I2C_ADDR_HAL(a7)    ((uint16_t)((a7) << 1))

#define LCD_I2C_TIMEOUT_MS      5U      /* Thời gian chờ tối đa cho 1 lần truyền I2C  */
#define LCD_BACKLIGHT_DEFAULT   1U      /* 1 = bật đèn nền khi khởi động              */

/* Ánh xạ chân PCF8574 -> LCD. Đây là cách đấu phổ biến nhất của module I2C 1602:
 *   P0=RS  P1=RW  P2=E  P3=Đèn nền  P4..P7 = D4..D7
 * Nếu module của bạn đấu khác, chỉ cần sửa 4 macro dưới (dữ liệu giữ ở P4..P7). */
#define LCD_PIN_RS              0x01U
#define LCD_PIN_RW              0x02U   /* luôn = 0 (chỉ ghi, không đọc LCD)          */
#define LCD_PIN_E               0x04U
#define LCD_PIN_BL              0x08U

/* Kích thước LCD */
#define LCD_COLS                16U
#define LCD_ROWS                2U

/* Thời gian cho khởi tạo / thử lại / hộp thoại */
#define LCD_POWERUP_MS          50U     /* Chờ LCD ổn định sau cấp nguồn (>40 ms)     */
#define LCD_RETRY_MS            500U    /* Chờ bao lâu rồi khởi tạo lại khi lỗi I2C   */
#define LCD_LEAVE_TIMEOUT_MS    3000U   /* Hộp thoại "Lưu/Hủy" tự đóng sau thời gian này */

/* Giới hạn giá trị cài đặt */
#define LCD_YEAR_MIN            2000U
#define LCD_YEAR_MAX            2099U
#define LCD_TEMP_THR_MAX        60U     /* Ngưỡng nhiệt độ tối đa (độ C)              */
#define LCD_PERCENT_MAX         100U    /* Ngưỡng độ ẩm tối đa (%)                    */
#define LCD_SCHEDULE_COUNT      2U

/* ===================== KIỂU DỮ LIỆU ===================== */

/** Bốn màn hình giao diện (trạng thái của máy trạng thái giao diện) */
typedef enum {
    LCD_SCREEN_TIME = 0,
    LCD_SCREEN_SENSOR,
    LCD_SCREEN_THRESHOLD,
    LCD_SCREEN_SCHEDULE,
    LCD_SCREEN_COUNT
} LCD_Screen_t;

/** Ngày giờ */
typedef struct {
    uint8_t  day;
    uint8_t  month;
    uint16_t year;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
} LCD_Time_t;

/** Dữ liệu cảm biến (chỉ để hiển thị) */
typedef struct {
    uint8_t temp;    /* Nhiệt độ không khí, DHT11, độ C            */
    uint8_t humi;    /* Độ ẩm không khí, DHT11, %                  */
    uint8_t soil;    /* Độ ẩm đất, cảm biến SV1, %                 */
    uint8_t water;   /* Mực nước bồn chứa, %                       */
} LCD_Sensor_t;

/** Chế độ tưới */
typedef enum {
    LCD_PUMP_AUTO = 0,   /* Tự động theo ngưỡng */
    LCD_PUMP_MANUAL      /* Thủ công            */
} LCD_PumpMode_t;

/** Ngưỡng cài đặt và chế độ bơm */
typedef struct {
    uint8_t        temp;     /* Tg: ngưỡng nhiệt độ không khí             */
    uint8_t        humi;     /* Hg: ngưỡng độ ẩm không khí                */
    uint8_t        soil;     /* Mg: ngưỡng độ ẩm đất                      */
    LCD_PumpMode_t mode;     /* AUTO hoặc MANU                            */
    uint8_t        pumpOn;   /* Yêu cầu bơm ở chế độ MANU: 1 = ON, 0 = OFF */
} LCD_Setting_t;

/** Một lịch tưới */
typedef struct {
    uint8_t hour;
    uint8_t minute;
    uint8_t enable;          /* 1 = ON, 0 = OFF */
} LCD_Schedule_t;

/** Trạng thái của driver LCD */
typedef enum {
    LCD_DRV_INIT = 0,    /* Đang khởi tạo từng bước theo thời gian */
    LCD_DRV_READY,       /* Sẵn sàng, đang cập nhật màn hình       */
    LCD_DRV_ERROR        /* Lỗi I2C, đợi LCD_RETRY_MS rồi thử lại  */
} LCD_DrvState_t;

/** Handle: chứa toàn bộ dữ liệu của module (không dùng biến toàn cục). */
typedef struct {
    /* ---- Driver ---- */
    I2C_HandleTypeDef *hi2c;
    uint16_t       addr;                       /* Địa chỉ cho HAL (đã dịch trái 1 bit)  */
    uint8_t        backlight;
    uint8_t        blPending;                  /* Có yêu cầu đổi đèn nền chưa gửi        */
    LCD_DrvState_t drv;
    uint8_t        initStep;                   /* Bước khởi tạo hiện tại                  */
    uint32_t       tRef;                       /* Mốc thời gian bắt đầu chờ               */
    uint32_t       waitMs;                     /* Thời gian cần chờ từ tRef               */
    uint16_t       errCount;                   /* Số lần lỗi I2C (để gỡ lỗi)              */
    char           shown[LCD_ROWS][LCD_COLS];  /* Nội dung đang có trên LCD               */
    char           want[LCD_ROWS][LCD_COLS];   /* Nội dung muốn hiển thị                  */
    uint8_t        hwAddr;                     /* Địa chỉ DDRAM hiện tại (0xFF = chưa biết) */
    uint8_t        shownCtrl;                  /* Lệnh Display Control đang áp dụng       */
    uint8_t        curStyle;                   /* 0 = ẩn, 1 = gạch chân, 2 = nhấp nháy    */
    uint8_t        curRow;
    uint8_t        curCol;

    /* ---- Dữ liệu ---- */
    LCD_Time_t     time;
    LCD_Sensor_t   sensor;
    LCD_Setting_t  setting;
    LCD_Schedule_t sched[LCD_SCHEDULE_COUNT];
    uint8_t        timeSaved;                  /* Người dùng vừa lưu giờ mới             */

    /* ---- Giao diện ---- */
    LCD_Screen_t   screen;
    uint8_t        editing;                    /* 1 = đang trong phiên chỉnh sửa          */
    uint8_t        field;                      /* Trường đang chọn                         */
    uint8_t        schedIdx;                   /* Lịch đang chọn (0 hoặc 1)                */
    uint8_t        leavePending;               /* Đang hiện hộp thoại "Lưu/Hủy"            */
    uint32_t       leaveTick;
    uint8_t        dirty;                      /* Cần vẽ lại nội dung                      */
    LCD_Time_t     editTime;                   /* Bản sao đang sửa (chưa lưu)              */
    LCD_Setting_t  editSetting;
    LCD_Schedule_t editSched;
} LCD_Handle_t;

/* ===================== HÀM GIAO TIẾP ===================== */

/** Khởi tạo handle và bắt đầu quy trình khởi tạo LCD (không chặn).
 *  @param addr7bit  địa chỉ I2C 7 bit (vd LCD_I2C_ADDR_7BIT) */
void LCD_Init(LCD_Handle_t *h, I2C_HandleTypeDef *hi2c, uint8_t addr7bit, uint32_t now);

/** Gọi mỗi vòng lặp: chạy khởi tạo, vẽ lại màn hình khi cần, xử lý lỗi I2C. */
void LCD_Task(LCD_Handle_t *h, uint32_t now);

/** Đưa 1 sự kiện nút nhấn vào máy trạng thái giao diện. */
void LCD_HandleEvent(LCD_Handle_t *h, Btn_Event_t evt, uint32_t now);

/* ---- Đưa dữ liệu vào LCD (dùng cho cảm biến / RTC thật sau này) ---- */
void LCD_SetTime(LCD_Handle_t *h, const LCD_Time_t *t);
void LCD_UpdateSensor(LCD_Handle_t *h, const LCD_Sensor_t *s);
void LCD_SetSetting(LCD_Handle_t *h, const LCD_Setting_t *s);
void LCD_SetSchedule(LCD_Handle_t *h, uint8_t idx, const LCD_Schedule_t *s);

/* ---- Lấy dữ liệu người dùng đã cài (dùng cho module bơm / RTC sau này) ---- */
const LCD_Setting_t  *LCD_GetSetting(const LCD_Handle_t *h);
const LCD_Schedule_t *LCD_GetSchedule(const LCD_Handle_t *h, uint8_t idx);

/** Trả về 1 (một lần duy nhất) nếu người dùng vừa lưu giờ mới; giờ mới được chép vào *out.
 *  Dùng để ghi xuống RTC thật sau này. */
uint8_t LCD_TimeSaved(LCD_Handle_t *h, LCD_Time_t *out);

void    LCD_SetBacklight(LCD_Handle_t *h, uint8_t on);
uint8_t LCD_IsReady(const LCD_Handle_t *h);

#ifdef __cplusplus
}
#endif

#endif /* LCD_I2C_H */

/**
 * @file    lcd.c
 * @brief   Driver LCD 16x02 qua PCF8574 (I2C) + máy trạng thái giao diện 4 màn hình.
 *
 * File gồm 3 phần:
 *   PHẦN 1 - DRIVER : gửi lệnh/dữ liệu, khởi tạo theo thời gian, chỉ ghi ký tự thay đổi.
 *   PHẦN 2 - GIAO DIỆN : hàm Render() dựng nội dung từng màn hình.
 *   PHẦN 3 - SỰ KIỆN  : xử lý nút nhấn cho từng màn hình, chỉnh sửa và lưu dữ liệu.
 *
 * Ý nghĩa nhãn trên LCD (do LCD chỉ có 16 cột nên phải viết tắt):
 *   Màn hình 2:  Temp = nhiệt độ không khí (DHT11)     H = độ ẩm không khí (DHT11)
 *                Soil = độ ẩm đất (cảm biến SV1)       W = mực nước bồn chứa
 *   Màn hình 3:  Tg = ngưỡng nhiệt độ   Hg = ngưỡng độ ẩm không khí   Mg = ngưỡng độ ẩm đất
 *                AUTO = tưới tự động    MANU ON/OFF = tưới thủ công, bơm bật/tắt
 *   Màn hình 4:  Lich1 / Lich2 = lịch tưới số 1 / số 2, theo sau là giờ:phút và ON/OFF
 */
#include "lcd_i2c.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* =====================================================================
 * PHẦN 1 - DRIVER
 * ===================================================================== */

/* Các bước khởi tạo HD44780 chế độ 4 bit (theo datasheet).
 * Mỗi bước gửi xong phải chờ waitMs rồi mới gửi bước kế tiếp - việc chờ do LCD_Task()
 * kiểm tra bằng HAL_GetTick, KHÔNG dùng delay.
 * HAL_GetTick có độ phân giải 1 ms nên thời gian chờ thật nằm trong (n-1..n] ms;
 * vì vậy chọn n lớn hơn mức tối thiểu của datasheet ít nhất 1 ms. */
typedef struct {
    uint8_t val;          /* Lệnh (hoặc nibble cao nếu nibbleOnly)       */
    uint8_t nibbleOnly;   /* 1 = chỉ gửi 4 bit cao (giai đoạn đầu)       */
    uint8_t waitMs;       /* Thời gian chờ sau bước này                  */
} InitStep_t;

static const InitStep_t kInitSeq[] = {
    { 0x30, 1, 6 },   /* Reset lần 1                    : chờ > 4,1 ms  */
    { 0x30, 1, 2 },   /* Reset lần 2                    : chờ > 100 us  */
    { 0x30, 1, 2 },   /* Reset lần 3                                     */
    { 0x20, 1, 2 },   /* Chuyển sang giao tiếp 4 bit                     */
    { 0x28, 0, 2 },   /* Function set: 4 bit, 2 dòng, font 5x8           */
    { 0x08, 0, 2 },   /* Display OFF                                     */
    { 0x01, 0, 4 },   /* Clear display (duy nhất 1 lần): cần > 1,52 ms   */
    { 0x06, 0, 2 },   /* Entry mode: địa chỉ tự tăng, không dịch màn hình */
    { 0x0C, 0, 2 },   /* Display ON, ẩn con trỏ                          */
};
#define INIT_STEPS  ((uint8_t)(sizeof(kInitSeq) / sizeof(kInitSeq[0])))

/* Gửi 1 byte (hoặc 1 nibble) tới HD44780 qua PCF8574.
 * Giao tiếp 4 bit: mỗi nibble cần 1 xung E (E=1 rồi E=0), nên 1 byte = 4 byte I2C.
 * Toàn bộ gửi bằng 1 lần HAL_I2C_Master_Transmit có timeout hữu hạn.
 * Mỗi lần gửi mất > 37 us nên các lệnh thường không cần chờ thêm. */
static HAL_StatusTypeDef LcdSend(LCD_Handle_t *h, uint8_t val, uint8_t rs, uint8_t nibbleOnly)
{
    uint8_t buf[4];
    uint8_t n = 0;
    uint8_t ctl = (uint8_t)((rs ? LCD_PIN_RS : 0U) | (h->backlight ? LCD_PIN_BL : 0U));
    uint8_t hi  = (uint8_t)((val & 0xF0U) | ctl);

    buf[n++] = (uint8_t)(hi | LCD_PIN_E);
    buf[n++] = hi;
    if (!nibbleOnly) {
        uint8_t lo = (uint8_t)(((val << 4) & 0xF0U) | ctl);
        buf[n++] = (uint8_t)(lo | LCD_PIN_E);
        buf[n++] = lo;
    }
    return HAL_I2C_Master_Transmit(h->hi2c, h->addr, buf, n, LCD_I2C_TIMEOUT_MS);
}

/* Lỗi I2C: chuyển sang LCD_DRV_ERROR, chương trình chính vẫn chạy bình thường */
static void Fail(LCD_Handle_t *h, uint32_t now)
{
    h->drv  = LCD_DRV_ERROR;
    h->tRef = now;
    if (h->errCount < 0xFFFFU) {
        h->errCount++;
    }
}

/* Bắt đầu (hoặc bắt đầu lại) quy trình khởi tạo: chờ LCD ổn định rồi chạy kInitSeq */
static void StartInit(LCD_Handle_t *h, uint32_t now)
{
    h->drv      = LCD_DRV_INIT;
    h->initStep = 0;
    h->tRef     = now;
    h->waitMs   = LCD_POWERUP_MS;
}

/* Gửi TỐI ĐA 1 thao tác tới LCD cho mỗi lần gọi, theo thứ tự ưu tiên:
 *   1. Đèn nền  2. Ký tự khác nhau đầu tiên giữa want và shown (đặt địa chỉ rồi ghi ký tự)
 *   3. Đặt vị trí con trỏ  4. Kiểu con trỏ.
 * Nhờ so sánh từng ký tự nên không cần xóa màn hình -> không nhấp nháy, ít lưu lượng I2C. */
static HAL_StatusTypeDef Flush(LCD_Handle_t *h)
{
    if (h->blPending) {
        uint8_t b = h->backlight ? LCD_PIN_BL : 0U;
        h->blPending = 0;
        return HAL_I2C_Master_Transmit(h->hi2c, h->addr, &b, 1, LCD_I2C_TIMEOUT_MS);
    }

    for (uint8_t r = 0; r < LCD_ROWS; r++) {
        for (uint8_t c = 0; c < LCD_COLS; c++) {
            if (h->want[r][c] == h->shown[r][c]) {
                continue;
            }
            uint8_t addr = (uint8_t)((r ? 0x40U : 0x00U) + c);   /* dòng 2 bắt đầu ở 0x40 */
            if (h->hwAddr != addr) {                              /* cần đặt lại địa chỉ DDRAM */
                h->hwAddr = addr;
                return LcdSend(h, (uint8_t)(0x80U | addr), 0, 0);
            }
            h->shown[r][c] = h->want[r][c];
            h->hwAddr++;                                          /* LCD tự tăng địa chỉ */
            return LcdSend(h, (uint8_t)h->want[r][c], 1, 0);
        }
    }

    /* Nội dung đã khớp -> xử lý con trỏ */
    uint8_t ctrl = 0x0CU;                          /* Display ON, không con trỏ */
    if (h->curStyle == 1U)      { ctrl |= 0x02U; } /* gạch chân                 */
    else if (h->curStyle == 2U) { ctrl |= 0x03U; } /* gạch chân + nhấp nháy     */

    if (h->curStyle != 0U) {
        uint8_t addr = (uint8_t)((h->curRow ? 0x40U : 0x00U) + h->curCol);
        if (h->hwAddr != addr) {
            h->hwAddr = addr;
            return LcdSend(h, (uint8_t)(0x80U | addr), 0, 0);
        }
    }
    if (ctrl != h->shownCtrl) {
        h->shownCtrl = ctrl;
        return LcdSend(h, ctrl, 0, 0);
    }
    return HAL_OK;
}

/* =====================================================================
 * PHẦN 2 - GIAO DIỆN (dựng nội dung màn hình)
 * ===================================================================== */

/* Ghi 1 dòng vào bộ đệm want. vsnprintf tự cắt ở 16 ký tự nên không bao giờ tràn LCD;
 * phần còn thiếu được đệm bằng dấu cách để ghi đè nội dung cũ. */
static void PutLine(LCD_Handle_t *h, uint8_t row, const char *fmt, ...)
{
    char buf[LCD_COLS + 1U];
    va_list ap;
    uint8_t i = 0;

    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    while (i < LCD_COLS && buf[i] != '\0') {
        h->want[row][i] = buf[i];
        i++;
    }
    while (i < LCD_COLS) {
        h->want[row][i++] = ' ';
    }
}

static void SetCursor(LCD_Handle_t *h, const uint8_t pos[2], uint8_t style)
{
    h->curRow   = pos[0];
    h->curCol   = pos[1];
    h->curStyle = style;
}

/* Vị trí con trỏ {hàng, cột} của từng trường. Phải khớp với định dạng chuỗi trong Render().
 * Quy ước: trường số -> con trỏ ở chữ số CUỐI; trường chữ (AUTO/MANU, ON/OFF) -> ở chữ ĐẦU. */
static const uint8_t kPosTime[6][2]  = { {0,7}, {0,10}, {0,15}, {1,7}, {1,10}, {1,13} };
static const uint8_t kPosThr[5][2]   = { {0,4}, {0,12}, {1,5},  {1,8}, {1,13} };
static const uint8_t kPosSched[3][2] = { {1,7}, {1,10}, {1,12} };

/* Dựng lại toàn bộ nội dung (want) theo màn hình hiện tại. Chỉ gọi khi dirty = 1. */
static void Render(LCD_Handle_t *h)
{
    h->curStyle = 0;   /* mặc định: không hiển thị con trỏ / dấu chọn */

    switch (h->screen) {
    case LCD_SCREEN_TIME: {
        /* Khi đang sửa thì hiển thị bản sao đang sửa, ngược lại hiển thị giờ thật */
        const LCD_Time_t *t = h->editing ? &h->editTime : &h->time;
        PutLine(h, 0, "Ngay: %02u/%02u/%04u", t->day, t->month, t->year);
        PutLine(h, 1, "Gio:  %02u:%02u:%02u", t->hour, t->minute, t->second);
        if (h->editing) {
            SetCursor(h, kPosTime[h->field], 2);          /* nhấp nháy = đang sửa */
        }
        break;
    }
    case LCD_SCREEN_SENSOR: {
        const LCD_Sensor_t *s = &h->sensor;
        PutLine(h, 0, "Temp:%2uC H:%3u%%", s->temp, s->humi);
        PutLine(h, 1, "Soil:%3u%% W:%3u%%", s->soil, s->water);
        break;
    }
    case LCD_SCREEN_THRESHOLD: {
        const LCD_Setting_t *s = h->editing ? &h->editSetting : &h->setting;
        PutLine(h, 0, "Tg:%2uC Hg:%3u%%", s->temp, s->humi);
        if (s->mode == LCD_PUMP_AUTO) {
            PutLine(h, 1, "Mg:%3u%% AUTO", s->soil);
        } else {
            PutLine(h, 1, "Mg:%3u%% MANU %s", s->soil, s->pumpOn ? "ON" : "OFF");
        }
        /* Màn hình này luôn có trường đang chọn: gạch chân = chọn, nhấp nháy = đang sửa */
        SetCursor(h, kPosThr[h->field], h->editing ? 2 : 1);
        break;
    }
    case LCD_SCREEN_SCHEDULE:
    default: {
        const LCD_Schedule_t *s = h->editing ? &h->editSched : &h->sched[h->schedIdx];
        PutLine(h, 0, "%02u/%02u/%04u %02u:%02u",
                h->time.day, h->time.month, h->time.year, h->time.hour, h->time.minute);
        PutLine(h, 1, "Lich%u %02u:%02u %s",
                h->schedIdx + 1U, s->hour, s->minute, s->enable ? "ON" : "OFF");
        if (h->editing) {
            SetCursor(h, kPosSched[h->field], 2);
        }
        break;
    }
    }

    /* Đang hỏi "Lưu hay Hủy" khi người dùng bấm MODE lúc chưa lưu */
    if (h->leavePending) {
        PutLine(h, 1, "SET:Luu MODE:Huy");
        h->curStyle = 0;
    }
}

/* =====================================================================
 * PHẦN 3 - XỬ LÝ SỰ KIỆN NÚT NHẤN
 * ===================================================================== */

/* Tăng/giảm v trong [mn, mx]. wrap = 1: vượt biên thì quay vòng; wrap = 0: dừng ở biên. */
static int32_t Step(int32_t v, int32_t mn, int32_t mx, int8_t dir, uint8_t wrap)
{
    v += dir;
    if (v > mx) { v = wrap ? mn : mx; }
    if (v < mn) { v = wrap ? mx : mn; }
    return v;
}

static uint8_t DaysInMonth(uint8_t month, uint16_t year)
{
    static const uint8_t kDays[12] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
    uint8_t leap = (uint8_t)(((year % 4U == 0U) && (year % 100U != 0U)) || (year % 400U == 0U));

    if (month < 1U || month > 12U) {
        return 31;
    }
    return (uint8_t)((month == 2U && leap) ? 29U : kDays[month - 1U]);
}

/* Đưa ngày/tháng/năm về phạm vi hợp lệ (vd tháng 2 không có ngày 31) */
static void FixDate(LCD_Time_t *t)
{
    if (t->month < 1U)             { t->month = 1U; }
    if (t->month > 12U)            { t->month = 12U; }
    if (t->year < LCD_YEAR_MIN)    { t->year = LCD_YEAR_MIN; }
    if (t->year > LCD_YEAR_MAX)    { t->year = LCD_YEAR_MAX; }
    if (t->day < 1U)               { t->day = 1U; }
    if (t->day > DaysInMonth(t->month, t->year)) { t->day = DaysInMonth(t->month, t->year); }
}

static void AdjustTime(LCD_Time_t *t, uint8_t field, int8_t dir)
{
    switch (field) {
    case 0:  t->day    = (uint8_t)Step(t->day, 1, DaysInMonth(t->month, t->year), dir, 1); break;
    case 1:  t->month  = (uint8_t)Step(t->month, 1, 12, dir, 1); break;
    case 2:  t->year   = (uint16_t)Step(t->year, LCD_YEAR_MIN, LCD_YEAR_MAX, dir, 1); break;
    case 3:  t->hour   = (uint8_t)Step(t->hour, 0, 23, dir, 1); break;
    case 4:  t->minute = (uint8_t)Step(t->minute, 0, 59, dir, 1); break;
    default: t->second = (uint8_t)Step(t->second, 0, 59, dir, 1); break;
    }
    FixDate(t);   /* đổi tháng/năm có thể làm ngày vượt số ngày của tháng */
}

static void AdjustThreshold(LCD_Setting_t *s, uint8_t field, int8_t dir)
{
    if (field == 0U)      { s->temp = (uint8_t)Step(s->temp, 0, LCD_TEMP_THR_MAX, dir, 0); }
    else if (field == 1U) { s->humi = (uint8_t)Step(s->humi, 0, LCD_PERCENT_MAX, dir, 0); }
    else                  { s->soil = (uint8_t)Step(s->soil, 0, LCD_PERCENT_MAX, dir, 0); }
}

static void AdjustSchedule(LCD_Schedule_t *s, uint8_t field, int8_t dir)
{
    if (field == 0U)      { s->hour   = (uint8_t)Step(s->hour, 0, 23, dir, 1); }
    else if (field == 1U) { s->minute = (uint8_t)Step(s->minute, 0, 59, dir, 1); }
    else                  { s->enable = (uint8_t)!s->enable; }
}

/* Lưu bản sao đang sửa vào dữ liệu thật (chỉ trong RAM) rồi thoát chế độ sửa */
static void Commit(LCD_Handle_t *h)
{
    switch (h->screen) {
    case LCD_SCREEN_TIME:
        h->time = h->editTime;
        h->timeSaved = 1;                       /* báo cho main để ghi RTC sau này */
        break;
    case LCD_SCREEN_THRESHOLD:                   /* chỉ lưu 3 ngưỡng; chế độ/bơm không thuộc phiên sửa */
        h->setting.temp = h->editSetting.temp;
        h->setting.humi = h->editSetting.humi;
        h->setting.soil = h->editSetting.soil;
        break;
    case LCD_SCREEN_SCHEDULE:
        h->sched[h->schedIdx] = h->editSched;
        break;
    default:
        break;
    }
    h->editing = 0;
}

/* Chuyển sang màn hình kế tiếp. Mọi bản sao đang sửa (nếu có) bị bỏ. */
static void NextScreen(LCD_Handle_t *h)
{
    h->screen       = (LCD_Screen_t)((h->screen + 1U) % LCD_SCREEN_COUNT);
    h->editing      = 0;
    h->leavePending = 0;
    h->field        = 0;
}

/* ---- Màn hình 1: ngày giờ. Trường: 0 ngày, 1 tháng, 2 năm, 3 giờ, 4 phút, 5 giây ---- */
static void HandleTime(LCD_Handle_t *h, Btn_Event_t e)
{
    if (e == BTN_EVT_SET) {
        if (!h->editing) {
            h->editTime = h->time;
            FixDate(&h->editTime);          /* giá trị 0 (chưa đặt) -> đưa về giá trị hợp lệ nhỏ nhất */
            h->editing = 1;
            h->field   = 0;
        } else {
            Commit(h);
        }
    } else if (h->editing) {
        if (e == BTN_EVT_RIGHT) {
            h->field = (uint8_t)((h->field + 1U) % 6U);
        } else if (e == BTN_EVT_UP || e == BTN_EVT_DOWN) {
            AdjustTime(&h->editTime, h->field, (e == BTN_EVT_UP) ? 1 : -1);
        }
    }
}

/* ---- Màn hình 3: ngưỡng + chế độ bơm. Trường: 0 Tg, 1 Hg, 2 Mg, 3 chế độ, 4 trạng thái bơm ---- */
static void HandleThreshold(LCD_Handle_t *h, Btn_Event_t e)
{
    LCD_Setting_t *s = &h->setting;

    switch (e) {
    case BTN_EVT_RIGHT:
        if (h->editing) {
            h->field = (uint8_t)((h->field + 1U) % 3U);      /* đang sửa: chỉ đi qua 3 ngưỡng */
        } else {
            h->field = (uint8_t)((h->field + 1U) % 5U);
            if (h->field == 4U && s->mode != LCD_PUMP_MANUAL) {
                h->field = 0;                                 /* AUTO: không có trường bơm */
            }
        }
        break;

    case BTN_EVT_UP:
    case BTN_EVT_DOWN:
        if (h->editing) {
            AdjustThreshold(&h->editSetting, h->field, (e == BTN_EVT_UP) ? 1 : -1);
        }
        break;                                                /* ngoài chế độ sửa: bỏ qua */

    case BTN_EVT_SET:
        if (h->editing) {
            Commit(h);                                        /* (a) LƯU NGƯỠNG */
        } else if (h->field < 3U) {
            h->editSetting = *s;                              /* (a) BẮT ĐẦU SỬA NGƯỠNG */
            h->editing = 1;
        } else if (h->field == 3U) {                          /* (b) ĐỔI CHẾ ĐỘ AUTO <-> MANU */
            s->mode   = (s->mode == LCD_PUMP_AUTO) ? LCD_PUMP_MANUAL : LCD_PUMP_AUTO;
            s->pumpOn = 0;                                    /* luôn về OFF cho an toàn */
        } else if (s->mode == LCD_PUMP_MANUAL) {              /* (c) BẬT/TẮT BƠM - chỉ khi MANU */
            s->pumpOn = (uint8_t)!s->pumpOn;
        }
        break;

    default:
        break;
    }
}

/* ---- Màn hình 4: lịch tưới. Trường: 0 giờ, 1 phút, 2 ON/OFF ---- */
static void HandleSchedule(LCD_Handle_t *h, Btn_Event_t e)
{
    switch (e) {
    case BTN_EVT_UP:
    case BTN_EVT_DOWN:
        if (h->editing) {
            AdjustSchedule(&h->editSched, h->field, (e == BTN_EVT_UP) ? 1 : -1);
        } else {
            h->schedIdx = (e == BTN_EVT_UP) ? 0U : 1U;        /* UP = lịch 1, DOWN = lịch 2 */
        }
        break;

    case BTN_EVT_RIGHT:
        if (h->editing) {
            h->field = (uint8_t)((h->field + 1U) % 3U);
        }
        break;

    case BTN_EVT_SET:
        if (h->editing) {
            Commit(h);
        } else {
            h->editSched = h->sched[h->schedIdx];
            h->editing = 1;
            h->field   = 0;
        }
        break;

    default:
        break;
    }
}

void LCD_HandleEvent(LCD_Handle_t *h, Btn_Event_t evt, uint32_t now)
{
    if (evt == BTN_EVT_NONE) {
        return;
    }
    h->dirty = 1;

    /* (1) Đang hiện hộp thoại "SET:Luu MODE:Huy" (người dùng bấm MODE khi chưa lưu) */
    if (h->leavePending) {
        h->leavePending = 0;
        if (evt == BTN_EVT_SET) {            /* Lưu rồi sang màn hình kế */
            Commit(h);
            NextScreen(h);
        } else if (evt == BTN_EVT_MODE) {    /* Hủy phần đang sửa rồi sang màn hình kế */
            NextScreen(h);
        }
        return;                              /* phím khác: chỉ đóng hộp thoại, tiếp tục sửa */
    }

    /* (2) Nút MODE: đang sửa dở thì hỏi trước, không mất dữ liệu ngoài ý muốn */
    if (evt == BTN_EVT_MODE) {
        if (h->editing) {
            h->leavePending = 1;
            h->leaveTick    = now;
        } else {
            NextScreen(h);
        }
        return;
    }

    /* (3) Các phím còn lại do từng màn hình xử lý */
    switch (h->screen) {
    case LCD_SCREEN_TIME:      HandleTime(h, evt);      break;
    case LCD_SCREEN_THRESHOLD: HandleThreshold(h, evt); break;
    case LCD_SCREEN_SCHEDULE:  HandleSchedule(h, evt);  break;
    default:                   break;                   /* màn hình cảm biến: chỉ xem */
    }
}

/* =====================================================================
 * HÀM CÔNG KHAI
 * ===================================================================== */

void LCD_Init(LCD_Handle_t *h, I2C_HandleTypeDef *hi2c, uint8_t addr7bit, uint32_t now)
{
    memset(h, 0, sizeof(*h));            /* mọi dữ liệu = 0, chế độ bơm = AUTO */
    h->hi2c      = hi2c;
    h->addr      = LCD_I2C_ADDR_HAL(addr7bit);
    h->backlight = LCD_BACKLIGHT_DEFAULT;
    h->screen    = LCD_SCREEN_TIME;
    h->hwAddr    = 0xFF;
    memset(h->shown, ' ', sizeof(h->shown));
    memset(h->want,  ' ', sizeof(h->want));
    StartInit(h, now);
    h->dirty = 1;
}

void LCD_Task(LCD_Handle_t *h, uint32_t now)
{
    /* Hộp thoại "Lưu/Hủy" tự đóng sau LCD_LEAVE_TIMEOUT_MS (phép trừ an toàn khi tick tràn) */
    if (h->leavePending && (uint32_t)(now - h->leaveTick) >= LCD_LEAVE_TIMEOUT_MS) {
        h->leavePending = 0;
        h->dirty = 1;
    }

    switch (h->drv) {
    case LCD_DRV_INIT:
        if ((uint32_t)(now - h->tRef) < h->waitMs) {
            break;                                   /* chưa đủ thời gian chờ: về vòng lặp chính */
        }
        if (h->initStep < INIT_STEPS) {
            const InitStep_t *st = &kInitSeq[h->initStep];
            if (LcdSend(h, st->val, 0, st->nibbleOnly) != HAL_OK) {
                Fail(h, now);
                break;
            }
            h->tRef   = now;
            h->waitMs = st->waitMs;
            h->initStep++;
        } else {                                      /* đã xong toàn bộ bước khởi tạo */
            memset(h->shown, ' ', sizeof(h->shown));  /* sau Clear display, LCD đang trống */
            h->hwAddr    = 0xFF;
            h->shownCtrl = 0x0C;
            h->drv       = LCD_DRV_READY;
            h->dirty     = 1;
        }
        break;

    case LCD_DRV_READY:
        if (h->dirty) {
            h->dirty = 0;
            Render(h);                                /* dựng nội dung, chưa gửi ra LCD */
        }
        if (Flush(h) != HAL_OK) {                     /* gửi tối đa 1 byte */
            Fail(h, now);
        }
        break;

    case LCD_DRV_ERROR:
    default:
        if ((uint32_t)(now - h->tRef) >= LCD_RETRY_MS) {
            /* Khôi phục I2C rồi khởi tạo LCD lại từ đầu.
             * Lưu ý: nếu I2C1 dùng chung với thiết bị khác (RTC...), việc reset này
             * xảy ra giữa hai giao dịch nên không làm hỏng giao dịch đang chạy. */
            HAL_I2C_DeInit(h->hi2c);
            HAL_I2C_Init(h->hi2c);
            memset(h->shown, ' ', sizeof(h->shown));
            h->hwAddr = 0xFF;
            StartInit(h, now);
        }
        break;
    }
}

void LCD_SetTime(LCD_Handle_t *h, const LCD_Time_t *t)
{
    h->time  = *t;
    h->dirty = 1;
}

void LCD_UpdateSensor(LCD_Handle_t *h, const LCD_Sensor_t *s)
{
    h->sensor = *s;
    h->dirty  = 1;
}

void LCD_SetSetting(LCD_Handle_t *h, const LCD_Setting_t *s)
{
    h->setting = *s;
    if (h->setting.temp > LCD_TEMP_THR_MAX) { h->setting.temp = LCD_TEMP_THR_MAX; }
    if (h->setting.humi > LCD_PERCENT_MAX)  { h->setting.humi = LCD_PERCENT_MAX; }
    if (h->setting.soil > LCD_PERCENT_MAX)  { h->setting.soil = LCD_PERCENT_MAX; }
    if (h->setting.mode != LCD_PUMP_MANUAL) {
        h->setting.mode   = LCD_PUMP_AUTO;
        h->setting.pumpOn = 0;                       /* AUTO: không có yêu cầu bơm thủ công */
    } else {
        h->setting.pumpOn = (uint8_t)(h->setting.pumpOn ? 1U : 0U);
    }
    h->dirty = 1;
}

void LCD_SetSchedule(LCD_Handle_t *h, uint8_t idx, const LCD_Schedule_t *s)
{
    if (idx >= LCD_SCHEDULE_COUNT) {
        return;
    }
    h->sched[idx] = *s;
    if (h->sched[idx].hour > 23U)   { h->sched[idx].hour = 23U; }
    if (h->sched[idx].minute > 59U) { h->sched[idx].minute = 59U; }
    h->sched[idx].enable = (uint8_t)(s->enable ? 1U : 0U);
    h->dirty = 1;
}

const LCD_Setting_t *LCD_GetSetting(const LCD_Handle_t *h)
{
    return &h->setting;
}

const LCD_Schedule_t *LCD_GetSchedule(const LCD_Handle_t *h, uint8_t idx)
{
    return &h->sched[(idx >= LCD_SCHEDULE_COUNT) ? (LCD_SCHEDULE_COUNT - 1U) : idx];
}

uint8_t LCD_TimeSaved(LCD_Handle_t *h, LCD_Time_t *out)
{
    if (!h->timeSaved) {
        return 0;
    }
    h->timeSaved = 0;
    *out = h->time;
    return 1;
}

void LCD_SetBacklight(LCD_Handle_t *h, uint8_t on)
{
    h->backlight = (uint8_t)(on ? 1U : 0U);
    h->blPending = 1;
}

uint8_t LCD_IsReady(const LCD_Handle_t *h)
{
    return (uint8_t)(h->drv == LCD_DRV_READY);
}

/**
 * @file    btn.h
 * @brief   Module đọc 5 nút nhấn (MODE, UP, DOWN, SET, RIGHT) cho STM32F103C8T6 + HAL.
 *
 * Cách hoạt động:
 *   - Nút nối GPIO input pull-up: nhấn = mức THẤP, thả = mức CAO.
 *   - BTN_Scan() được gọi liên tục trong vòng lặp chính (không dùng delay, không chờ nút).
 *   - Chống dội bằng thời gian: mức đọc phải ổn định >= BTN_DEBOUNCE_MS mới được chấp nhận.
 *   - Mỗi lần nhấn hợp lệ tạo ĐÚNG 1 sự kiện, đẩy vào hàng đợi nhỏ.
 *     Giữ nút không tạo thêm sự kiện; phải thả rồi nhấn lại mới có sự kiện mới.
 *   - Module này KHÔNG gọi hàm LCD. Vòng lặp chính lấy sự kiện bằng BTN_GetEvent()
 *     rồi chuyển cho LCD_HandleEvent().
 */
#ifndef BTN_H
#define BTN_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f1xx_hal.h"

/* ===================== CẤU HÌNH (đổi ở đây nếu đấu dây khác) ===================== */
#define BTN_MODE_PORT    GPIOA
#define BTN_MODE_PIN     GPIO_PIN_0
#define BTN_UP_PORT      GPIOA
#define BTN_UP_PIN       GPIO_PIN_2
#define BTN_DOWN_PORT    GPIOA
#define BTN_DOWN_PIN     GPIO_PIN_3
#define BTN_SET_PORT     GPIOA
#define BTN_SET_PIN      GPIO_PIN_4
#define BTN_RIGHT_PORT   GPIOA
#define BTN_RIGHT_PIN    GPIO_PIN_5

#define BTN_DEBOUNCE_MS   30U             /* Thời gian chống dội (20..30 ms là hợp lý)  */
#define BTN_ACTIVE_LEVEL  GPIO_PIN_RESET  /* Mức logic khi nhấn (pull-up => mức thấp)   */
#define BTN_QUEUE_SIZE    8U              /* Số sự kiện tối đa chờ xử lý                */

/* ===================== KIỂU DỮ LIỆU ===================== */

/** Chỉ số của từng nút (dùng làm chỉ số mảng) */
typedef enum {
    BTN_ID_MODE = 0,
    BTN_ID_UP,
    BTN_ID_DOWN,
    BTN_ID_SET,
    BTN_ID_RIGHT,
    BTN_ID_COUNT
} Btn_Id_t;

/** Sự kiện "đã nhấn" gửi sang module LCD. Giá trị = Btn_Id_t + 1 (0 = không có sự kiện) */
typedef enum {
    BTN_EVT_NONE = 0,
    BTN_EVT_MODE,
    BTN_EVT_UP,
    BTN_EVT_DOWN,
    BTN_EVT_SET,
    BTN_EVT_RIGHT
} Btn_Event_t;

/** Trạng thái chống dội của một nút */
typedef struct {
    uint8_t  raw;      /* Mức đọc thô ở lần quét trước (1 = đang nhấn)            */
    uint8_t  stable;   /* Trạng thái đã chống dội (1 = đang nhấn)                  */
    uint32_t tChange;  /* Thời điểm (ms) mức thô thay đổi lần gần nhất             */
} Btn_State_t;

/* ===================== HÀM GIAO TIẾP ===================== */

/** Khởi tạo: đọc trạng thái hiện tại của các nút (nút đang bị giữ lúc khởi động sẽ không tạo sự kiện). */
void BTN_Init(uint32_t now);

/** Quét 5 nút, cập nhật chống dội, đẩy sự kiện nhấn vào hàng đợi. Gọi mỗi vòng lặp chính. */
void BTN_Scan(uint32_t now);

/** Lấy 1 sự kiện từ hàng đợi; trả về BTN_EVT_NONE nếu không còn sự kiện nào. Không chờ. */
Btn_Event_t BTN_GetEvent(void);

#ifdef __cplusplus
}
#endif

#endif /* BTN_H */

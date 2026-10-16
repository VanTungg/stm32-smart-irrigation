/**
 * @file    btn.c
 * @brief   Đọc nút nhấn, chống dội bằng thời gian (HAL_GetTick), tạo sự kiện nhấn.
 *
 * Thuật toán chống dội cho mỗi nút (không dùng delay):
 *   1. Đọc mức thô (raw). Nếu raw khác lần trước -> ghi lại thời điểm thay đổi (tChange).
 *   2. Nếu raw giữ nguyên và khác trạng thái ổn định (stable) đủ BTN_DEBOUNCE_MS
 *      -> chấp nhận: stable = raw.
 *   3. Chỉ khi stable chuyển sang "nhấn" mới phát sự kiện. Thả nút không phát gì.
 *
 * Hiệu thời gian dùng phép trừ số nguyên không dấu 32 bit: (now - tChange)
 * nên vẫn đúng khi bộ đếm HAL_GetTick tràn (sau ~49,7 ngày).
 */
#include "btn.h"

/* Bảng chân phần cứng, thứ tự phải khớp enum Btn_Id_t */
static const struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
} kBtnHw[BTN_ID_COUNT] = {
    { BTN_MODE_PORT,  BTN_MODE_PIN  },
    { BTN_UP_PORT,    BTN_UP_PIN    },
    { BTN_DOWN_PORT,  BTN_DOWN_PIN  },
    { BTN_SET_PORT,   BTN_SET_PIN   },
    { BTN_RIGHT_PORT, BTN_RIGHT_PIN },
};

static Btn_State_t s_btn[BTN_ID_COUNT];     /* Trạng thái chống dội từng nút */
static Btn_Event_t s_queue[BTN_QUEUE_SIZE]; /* Hàng đợi vòng chứa sự kiện    */
static uint8_t     s_head;                  /* Vị trí ghi                    */
static uint8_t     s_tail;                  /* Vị trí đọc                    */

/* Trả về 1 nếu nút đang ở mức "nhấn" */
static uint8_t ReadPressed(uint8_t id)
{
    return (HAL_GPIO_ReadPin(kBtnHw[id].port, kBtnHw[id].pin) == BTN_ACTIVE_LEVEL) ? 1U : 0U;
}

/* Đẩy sự kiện vào hàng đợi; nếu đầy thì bỏ sự kiện mới (hiếm khi xảy ra) */
static void Push(Btn_Event_t evt)
{
    uint8_t next = (uint8_t)((s_head + 1U) % BTN_QUEUE_SIZE);
    if (next != s_tail) {
        s_queue[s_head] = evt;
        s_head = next;
    }
}

void BTN_Init(uint32_t now)
{
    for (uint8_t i = 0; i < BTN_ID_COUNT; i++) {
        s_btn[i].raw     = ReadPressed(i);
        s_btn[i].stable  = s_btn[i].raw;   /* nút đang giữ lúc bật nguồn: chưa tính là nhấn */
        s_btn[i].tChange = now;
    }
    s_head = 0;
    s_tail = 0;
}

void BTN_Scan(uint32_t now)
{
    for (uint8_t i = 0; i < BTN_ID_COUNT; i++) {
        Btn_State_t *b = &s_btn[i];
        uint8_t raw = ReadPressed(i);

        if (raw != b->raw) {
            /* Mức đọc vừa đổi: bắt đầu đếm lại thời gian ổn định */
            b->raw = raw;
            b->tChange = now;
        } else if (raw != b->stable && (uint32_t)(now - b->tChange) >= BTN_DEBOUNCE_MS) {
            /* Mức đọc đã ổn định đủ lâu: chấp nhận */
            b->stable = raw;
            if (raw) {
                Push((Btn_Event_t)(i + 1U));   /* chỉ phát sự kiện khi NHẤN, không phát khi thả */
            }
        }
    }
}

Btn_Event_t BTN_GetEvent(void)
{
    Btn_Event_t evt = BTN_EVT_NONE;
    if (s_head != s_tail) {
        evt = s_queue[s_tail];
        s_tail = (uint8_t)((s_tail + 1U) % BTN_QUEUE_SIZE);
    }
    return evt;
}

#include "relay.h"

static Relay_State current_state = RELAY_OFF;

void Relay_Init(void) {
    // Tắt bơm khi khởi tạo hệ thống (Relay kích mức cao HIGH/thấp LOW tùy module, mặc định OFF)
    Relay_SetState(RELAY_OFF);
}

void Relay_SetState(Relay_State state) {
    current_state = state;
    if (state == RELAY_ON) {
        HAL_GPIO_WritePin(RELAY_GPIO_PORT, RELAY_PIN, GPIO_PIN_SET);   // Bật máy bơm
    } else {
        HAL_GPIO_WritePin(RELAY_GPIO_PORT, RELAY_PIN, GPIO_PIN_RESET); // Tắt máy bơm
    }
}

Relay_State Relay_GetState(void) {
    return current_state;
}

void Relay_Toggle(void) {
    if (current_state == RELAY_ON) {
        Relay_SetState(RELAY_OFF);
    } else {
        Relay_SetState(RELAY_ON);
    }
}
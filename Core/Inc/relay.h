#ifndef __RELAY_H
#define __RELAY_H

#include "stm32f1xx_hal.h"

// Định nghĩa chân cắm Relay (PB0)
#define RELAY_GPIO_PORT    GPIOB
#define RELAY_PIN          GPIO_PIN_0

typedef enum {
    RELAY_OFF = 0,
    RELAY_ON  = 1
} Relay_State;

void Relay_Init(void);
void Relay_SetState(Relay_State state);
Relay_State Relay_GetState(void);
void Relay_Toggle(void);

#endif /* __RELAY_H */
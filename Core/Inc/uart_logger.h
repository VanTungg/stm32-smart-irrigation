#ifndef __UART_LOGGER_H
#define __UART_LOGGER_H

#include "stm32f1xx_hal.h"
#include <stdio.h>
#include <stdarg.h>

typedef enum {
    LOG_LEVEL_INFO,  
    LOG_LEVEL_WARN, 
    LOG_LEVEL_ERROR 
} LogLevel;

void UART_Logger_Init(UART_HandleTypeDef *huart);
void UART_Logger_Log(LogLevel level, const char *format, ...);

#define LOG_I(fmt, ...) UART_Logger_Log(LOG_LEVEL_INFO,  fmt, ##__VA_ARGS__)
#define LOG_W(fmt, ...) UART_Logger_Log(LOG_LEVEL_WARN,  fmt, ##__VA_ARGS__)
#define LOG_E(fmt, ...) UART_Logger_Log(LOG_LEVEL_ERROR, fmt, ##__VA_ARGS__)

#endif /* __UART_LOGGER_H */
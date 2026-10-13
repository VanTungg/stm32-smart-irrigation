#include "uart_logger.h"

static UART_HandleTypeDef *p_huart = NULL;

void UART_Logger_Init(UART_HandleTypeDef *huart) {
    p_huart = huart;
}

void UART_Logger_Log(LogLevel level, const char *format, ...) {
    if (p_huart == NULL) return;

    char buffer[128];
    uint32_t tick = HAL_GetTick(); 
    int prefix_len = 0;

    switch (level) {
        case LOG_LEVEL_INFO:
            prefix_len = sprintf(buffer, "[%8lu ms] [INFO] ", (unsigned long)tick);
            break;
        case LOG_LEVEL_WARN:
            prefix_len = sprintf(buffer, "[%8lu ms] [WARN] ", (unsigned long)tick);
            break;
        case LOG_LEVEL_ERROR:
            prefix_len = sprintf(buffer, "[%8lu ms] [ERR ] ", (unsigned long)tick);
            break;
    }

    va_list args;
    va_start(args, format);
    int msg_len = vsnprintf(buffer + prefix_len, sizeof(buffer) - prefix_len - 3, format, args);
    va_end(args);

    int total_len = prefix_len + msg_len;
    buffer[total_len++] = '\r';
    buffer[total_len++] = '\n';
    buffer[total_len] = '\0';

    HAL_UART_Transmit(p_huart, (uint8_t *)buffer, total_len, 100);
}
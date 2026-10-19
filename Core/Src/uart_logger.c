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
    // Tính toán độ dài an toàn còn lại cho thông điệp (Trừ đi 3 byte cho \r, \n, \0)
    int max_msg_len = sizeof(buffer) - prefix_len - 3;
    int msg_len = vsnprintf(buffer + prefix_len, max_msg_len, format, args);
    va_end(args);

    // NẾU thông điệp dài hơn vùng trống, vsnprintf sẽ trả về chiều dài THỰC TẾ cần có (lớn hơn max_msg_len)
    // Ta phải cắt nó lại để không bị ghi tràn vùng nhớ
    if (msg_len > max_msg_len || msg_len < 0) {
        msg_len = max_msg_len - 1; 
    }

    int total_len = prefix_len + msg_len;
    buffer[total_len++] = '\r';
    buffer[total_len++] = '\n';
    buffer[total_len] = '\0';

    HAL_UART_Transmit(p_huart, (uint8_t *)buffer, total_len, 100);
}
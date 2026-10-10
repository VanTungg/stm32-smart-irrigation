#ifndef __RTC_H
#define __RTC_H
#include "main.h"
extern volatile uint8_t rtc_hour;
extern volatile uint8_t rtc_minute;
extern volatile uint8_t rtc_second;
void RTC_Init_Module(void);
void RTC_Update(void);
#endif 
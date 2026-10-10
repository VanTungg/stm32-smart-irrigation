#include "rtc.h"
extern RTC_HandleTypeDef hrtc;
volatile uint8_t rtc_hour = 0;
volatile uint8_t rtc_minute = 0;
volatile uint8_t rtc_second = 0;
static uint32_t last_rtc_tick = 0;
void RTC_Init_Module(void)
{
    RTC_TimeTypeDef sTime = {0};
    sTime.Hours = 3;
    sTime.Minutes = 45;
    sTime.Seconds = 0;
    HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
    RTC_DateTypeDef sDate = {0};
    sDate.WeekDay = RTC_WEEKDAY_MONDAY;
    sDate.Month = RTC_MONTH_JANUARY;
    sDate.Date = 1;
    sDate.Year = 26;
    HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
}

void RTC_Update(void)
{
    if (HAL_GetTick() - last_rtc_tick >= 500)
    {
        last_rtc_tick = HAL_GetTick();
        RTC_TimeTypeDef sTime;
        RTC_DateTypeDef sDate;
        if (HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN) == HAL_OK)
        {
            HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN); 
            rtc_hour = sTime.Hours;
            rtc_minute = sTime.Minutes;
            rtc_second = sTime.Seconds;
        }
    }
}
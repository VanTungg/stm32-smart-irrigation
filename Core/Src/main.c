/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "relay.h"
#include "dht11.h"
#include "uart_logger.h"
#include "soil_sensor.h"
#include "rtc.h"
#include "btn.h"
#include "lcd_i2c.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
ADC_HandleTypeDef hadc2;
I2C_HandleTypeDef hi2c1;
RTC_HandleTypeDef hrtc;
TIM_HandleTypeDef htim2;
UART_HandleTypeDef huart1;
/* USER CODE BEGIN PV */
DHT11_Data dht_sensor;
uint8_t dht_status = 0;
SoilSensor soil;
uint32_t adc_water_raw = 0;
float water_level_pct = 0.0f; 

uint32_t last_sensor_time = 0;
static LCD_Handle_t lcd;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_RTC_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_ADC2_Init(void);
/* USER CODE BEGIN PFP */
static float Read_Water_Level(ADC_HandleTypeDef *hadc) {
    uint32_t sum = 0;
    for (uint8_t i = 0; i < 10; i++) {
        HAL_ADC_Start(hadc);
        if (HAL_ADC_PollForConversion(hadc, 10) == HAL_OK) {
            sum += HAL_ADC_GetValue(hadc);
        }
        HAL_ADC_Stop(hadc);
    }
    // Giả sử 0-4095 tương ứng 0-100%
    float pct = ((float)(sum / 10) / 4095.0f) * 100.0f;
    return (pct > 100.0f) ? 100.0f : pct;
}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_RTC_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  MX_ADC2_Init();
  /* USER CODE BEGIN 2 */
  Relay_Init();
  DHT11_Init();
  HAL_ADC_Start(&hadc1);
  UART_Logger_Init(&huart1);
  LCD_Init(&lcd, &hi2c1, 0x20, HAL_GetTick());
  LOG_I("=== HE THONG TUOI CAY TU DONG KHOT DONG ===");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
   uint32_t now = HAL_GetTick();

    /* ====================================================================
     * TÁC VỤ 1: ĐA NHIỆM GIAO DIỆN (Chạy liên tục, KHÔNG dùng Delay)
     * ==================================================================== */
    BTN_Scan(now);
    Btn_Event_t evt = BTN_GetEvent();
    if (evt != BTN_EVT_NONE) {
        LCD_HandleEvent(&lcd, evt, now);
    }
    LCD_Task(&lcd, now);
    RTC_Process(); // Xử lý nhịp xung 10s ON / 5s OFF

    /* Đồng bộ LCD -> RTC (Nếu người dùng chỉnh giờ trên màn hình) */
    LCD_Time_t newTime;
    if (LCD_TimeSaved(&lcd, &newTime)) {
        RTC_TimeTypeDef sTime = { .Hours = newTime.hour, .Minutes = newTime.minute, .Seconds = newTime.second };
        HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN);
        RTC_DateTypeDef sDate = { .Date = newTime.day, .Month = newTime.month, .Year = (uint8_t)(newTime.year - 2000), .WeekDay = RTC_WEEKDAY_MONDAY };
        HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
        LOG_I("Da cap nhat gio RTC tu LCD");
    }
        /* ĐỒNG BỘ LỊCH TỪ LCD SANG RTC */
        const LCD_Schedule_t* sched1 = LCD_GetSchedule(&lcd, 0);
        const LCD_Schedule_t* sched2 = LCD_GetSchedule(&lcd, 1);
        RTC_DatLich1(sched1->hour, sched1->minute, sched1->enable);
        RTC_DatLich2(sched2->hour, sched2->minute, sched2->enable);

    /* ====================================================================
     * TÁC VỤ 2: QUÉT CẢM BIẾN & XỬ LÝ LOGIC (Mỗi 1000ms)
     * ==================================================================== */
    if (now - last_sensor_time >= 1000) {
        last_sensor_time = now;

        // 1. Đọc dữ liệu cảm biến
        dht_status = DHT11_Read(&dht_sensor);
        Soil_Read(&hadc1, &soil);
        water_level_pct = Read_Water_Level(&hadc2);

        float T = dht_sensor.temperature;
        float H_air = dht_sensor.humidity;
        float M_soil = soil.soil_moisture;

        // 2. Đẩy dữ liệu hiển thị lên LCD
        LCD_Sensor_t realSensors = {
            .temp = (uint8_t)T, .humi = (uint8_t)H_air,
            .soil = (uint8_t)M_soil, .water = (uint8_t)water_level_pct
        };
        LCD_UpdateSensor(&lcd, &realSensors);

        // Đẩy giờ RTC lên LCD
        RTC_TimeTypeDef sTime;
        if (HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN) == HAL_OK) {
            RTC_DateTypeDef sDate; HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);
            LCD_Time_t currentTime = {
                .day = (sDate.Date == 0) ? 1 : sDate.Date, .month = (sDate.Month == 0) ? 1 : sDate.Month, .year = 2000 + sDate.Year,
                .hour = sTime.Hours, .minute = sTime.Minutes, .second = sTime.Seconds
            };
            LCD_SetTime(&lcd, &currentTime);
        }

        /* =================================================================
         * 3. LOGIC ĐIỀU KHIỂN BƠM THEO CẤP ĐỘ ƯU TIÊN 
         * ================================================================= */
        const LCD_Setting_t* set = LCD_GetSetting(&lcd);

        // Lấy ngưỡng động từ cài đặt trên LCD (Mặc định nếu chưa cài: 40% đất, 32C, 30% ẩm)
        float threshold_low = (float)(set->soil > 0 ? set->soil : 40); 
        float threshold_high = threshold_low + 20.0f; // Tạo Hysteresis dải 20%
        if (threshold_high > 100.0f) threshold_high = 100.0f;

        float temp_limit = (float)(set->temp > 0 ? set->temp : 32);
        float humi_limit = (float)(set->humi > 0 ? set->humi : 30);

        // --- ƯU TIÊN 0: BẢO VỆ PHẦN CỨNG BẮT BUỘC (Cao nhất) ---
        if (dht_status == 0 || soil.status == SOIL_ERROR) {
            Relay_SetState(RELAY_OFF);
            RTC_DungTuoi();
            LOG_E("LOI CAM BIEN: Ngat bom an toan.");
        }
        else if (water_level_pct < 10.0f) {
            Relay_SetState(RELAY_OFF);
            RTC_DungTuoi();
            LOG_E("LOI BE NUOC: Muc nuoc %.1f%% < 10%% -> Khoa bom chong chay kho.", water_level_pct);
        }
        else if (T > 38.0f) {
            Relay_SetState(RELAY_OFF);
            RTC_DungTuoi();
            LOG_W("CANH BAO DAC BIET: Nhiet do %.1f > 38C -> Tat bom chong soc nhiet!", T);
        }
        
        // --- ƯU TIÊN 1: BƠM THỦ CÔNG (Từ màn hình LCD) ---
        else if (set->mode == LCD_PUMP_MANUAL) {
            Relay_SetState(set->pumpOn ? RELAY_ON : RELAY_OFF);
            LOG_I("MANUAL MODE: Bom dang %s", set->pumpOn ? "BAT" : "TAT");
        }
        
        // --- ƯU TIÊN 2.A: ĐẠT MỤC TIÊU ĐỘ ẨM KHÓA BƠM HẲN ---
        else if (M_soil >= threshold_high) {
            Relay_SetState(RELAY_OFF);
            RTC_DungTuoi();
            
            if (T <= temp_limit && H_air >= humi_limit)      LOG_I("M>=%.0f%% -> Tat bom (Khong canh bao)", threshold_high);
            else if (T <= temp_limit && H_air < humi_limit)  LOG_W("M>=%.0f%% -> Tat bom (CB: Khong khi kho)", threshold_high);
            else if (T > temp_limit && H_air >= humi_limit)  LOG_W("M>=%.0f%% -> Tat bom (CB: Nhiet do cao)", threshold_high);
            else                                             LOG_W("M>=%.0f%% -> Tat bom (CB: Nong & Kho)", threshold_high);
        }

        // --- ƯU TIÊN 2.B: LỊCH TƯỚI XUNG NHỊP RTC ---
        else if (RTC_KiemTraPhienTuoiDangChay()) {
            Relay_State rtc_state = RTC_KiemTraBomDangChay() ? RELAY_ON : RELAY_OFF;
            Relay_SetState(rtc_state);
            LOG_I("AUTO-RTC: Chay theo nhip (%s)", rtc_state == RELAY_ON ? "10s ON" : "5s OFF");
        }

        // --- ƯU TIÊN 3: LOGIC CẢM BIẾN TỰ ĐỘNG ---
        else {
            if (M_soil < threshold_low) {
                Relay_SetState(RELAY_ON);
                if (T <= temp_limit && H_air >= humi_limit)      LOG_I("M<%.0f%% -> Bom (Khong canh bao)", threshold_low);
                else if (T <= temp_limit && H_air < humi_limit)  LOG_W("M<%.0f%% -> Bom (CB: Khong khi kho)", threshold_low);
                else if (T > temp_limit && H_air >= humi_limit)  LOG_W("M<%.0f%% -> Bom (CB: Nhiet do cao)", threshold_low);
                else                                             LOG_W("M<%.0f%% -> Bom (CB: Nong & Kho)", threshold_low);
            }
            else { // Vùng Hysteresis (Đang giữa mức thấp và cao)
                if (T <= temp_limit && H_air >= humi_limit) {
                    Relay_State current = Relay_GetState();
                    LOG_I("TH5: M(%.0f-%.0f%%) -> Giu nguyen (%s) (Tiep tuc theo doi)", threshold_low, threshold_high, current == RELAY_ON ? "Bom" : "Tat");
                }
                else {
                    Relay_SetState(RELAY_ON);
                    if (T <= temp_limit && H_air < humi_limit)       LOG_W("M(%.0f-%.0f%%) -> Bom (CB: Khong khi kho)", threshold_low, threshold_high);
                    else if (T > temp_limit && H_air >= humi_limit)  LOG_W("M(%.0f-%.0f%%) -> Bom (CB: Nhiet do cao)", threshold_low, threshold_high);
                    else                                             LOG_W("M(%.0f-%.0f%%) -> Bom (CB: Nong & Kho)", threshold_low, threshold_high);
                }
            }
        }
    }
  }

  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE|RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_RTC|RCC_PERIPHCLK_ADC;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSE;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_1;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief ADC2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC2_Init(void)
{

  /* USER CODE BEGIN ADC2_Init 0 */

  /* USER CODE END ADC2_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC2_Init 1 */

  /* USER CODE END ADC2_Init 1 */

  /** Common config
  */
  hadc2.Instance = ADC2;
  hadc2.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc2.Init.ContinuousConvMode = DISABLE;
  hadc2.Init.DiscontinuousConvMode = DISABLE;
  hadc2.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc2.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc2.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_6;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_239CYCLES_5;
  if (HAL_ADC_ConfigChannel(&hadc2, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC2_Init 2 */

  /* USER CODE END ADC2_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{
  
  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef DateToUpdate = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.AsynchPrediv = RTC_AUTO_1_SECOND;
  hrtc.Init.OutPut = RTC_OUTPUTSOURCE_ALARM;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
/** Initialize RTC and set the Time and Date ONLY IF first boot
  */
  if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR1) != 0x32F2)
  {
      sTime.Hours = 0x0;
      sTime.Minutes = 0x0;
      sTime.Seconds = 0x0;

      if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
      {
        Error_Handler();
      }
      
      DateToUpdate.WeekDay = RTC_WEEKDAY_MONDAY;
      DateToUpdate.Month = RTC_MONTH_JANUARY;
      DateToUpdate.Date = 0x1;
      DateToUpdate.Year = 0x0;

      if (HAL_RTC_SetDate(&hrtc, &DateToUpdate, RTC_FORMAT_BCD) != HAL_OK)
      {
        Error_Handler();
      }
      
      /* Lưu dấu ấn vào Backup Register để các lần Reset sau không bị ghi đè giờ */
      HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR1, 0x32F2);
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 71;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 65535;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_8, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA0 PA2 PA3 PA4
                           PA5 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_4
                          |GPIO_PIN_5;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB8 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_8;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */

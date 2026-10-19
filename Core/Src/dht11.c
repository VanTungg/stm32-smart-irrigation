#include "dht11.h"

static void delay_us(uint32_t us) {
    uint32_t startTicks = DWT->CYCCNT;
    uint32_t targetTicks = us * (SystemCoreClock / 1000000U);
    while ((DWT->CYCCNT - startTicks) < targetTicks);
}

static void DHT11_SetPinOutput(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

static void DHT11_SetPinInput(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = DHT11_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(DHT11_PORT, &GPIO_InitStruct);
}

void DHT11_Init(void) {
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

static uint8_t DHT11_StartSignal(void) {
    DHT11_SetPinOutput();
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_RESET);
    HAL_Delay(18); 
    HAL_GPIO_WritePin(DHT11_PORT, DHT11_PIN, GPIO_PIN_SET);
    delay_us(30); 

    DHT11_SetPinInput();

    delay_us(40);
    if (!(HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN))) {
        delay_us(80);
        if ((HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN))) {
            delay_us(50);
            return 1;
        }
    }
    return 0; // Lỗi phản hồi
}

static uint8_t DHT11_ReadByte(void) {
    uint8_t i, data = 0;
    uint32_t timeout; // Biến chống treo
    
    for (i = 0; i < 8; i++) {
        timeout = 10000;
        while (!(HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN))) {
            if (--timeout == 0) return 0; // Thoát nếu kẹt ở mức 0
        }
        
        delay_us(40);
        
        if (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN)) {
            data |= (1 << (7 - i));
            timeout = 10000;
            while (HAL_GPIO_ReadPin(DHT11_PORT, DHT11_PIN)) {
                if (--timeout == 0) return 0; // Thoát nếu kẹt ở mức 1
            }
        }
    }
    return data;
}

uint8_t DHT11_Read(DHT11_Data *data) {
    uint8_t rh_byte1, rh_byte2, temp_byte1, temp_byte2, checksum;

    if (DHT11_StartSignal()) {
        rh_byte1   = DHT11_ReadByte();
        rh_byte2   = DHT11_ReadByte();
        temp_byte1 = DHT11_ReadByte();
        temp_byte2 = DHT11_ReadByte();
        checksum   = DHT11_ReadByte();

        if ((rh_byte1 + rh_byte2 + temp_byte1 + temp_byte2) == checksum) {
            data->humidity    = (float)rh_byte1 + (float)rh_byte2 / 10.0f;
            data->temperature = (float)temp_byte1 + (float)temp_byte2 / 10.0f;
            return 1;
        }
    }
    return 0;
}
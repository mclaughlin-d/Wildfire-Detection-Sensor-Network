#include "dht22.h"

/* ── Private variables ───────────────────────────────────────────────────── */
static uint32_t pMillis, cMillis;

/* ── µs delay using TIM3 ─────────────────────────────────────────────────── */
static void microDelay(uint16_t delay)
{
    __HAL_TIM_SET_COUNTER(&DHT22_TIMER, 0);
    while (__HAL_TIM_GET_COUNTER(&DHT22_TIMER) < delay);
}

/* ── Init: start timer, idle pin HIGH ───────────────────────────────────── */
void DHT22_Init(void)
{
    HAL_TIM_Base_Start(&DHT22_TIMER);
    HAL_GPIO_WritePin(DHT22_PORT, DHT22_PIN, GPIO_PIN_SET);
}

/* ── Send start signal, return 1 if sensor responds ─────────────────────── */
uint8_t DHT22_Start(void)
{
    uint8_t Response = 0;
    GPIO_InitTypeDef GPIO_InitStructPrivate = {0};

    GPIO_InitStructPrivate.Pin   = DHT22_PIN;
    GPIO_InitStructPrivate.Mode  = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStructPrivate.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStructPrivate.Pull  = GPIO_NOPULL;
    HAL_GPIO_Init(DHT22_PORT, &GPIO_InitStructPrivate);

    HAL_GPIO_WritePin(DHT22_PORT, DHT22_PIN, 0);
    microDelay(1300);
    HAL_GPIO_WritePin(DHT22_PORT, DHT22_PIN, 1);
    microDelay(30);

    GPIO_InitStructPrivate.Mode = GPIO_MODE_INPUT;
    GPIO_InitStructPrivate.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DHT22_PORT, &GPIO_InitStructPrivate);

    microDelay(40);
    if (!(HAL_GPIO_ReadPin(DHT22_PORT, DHT22_PIN)))
    {
        microDelay(80);
        if ((HAL_GPIO_ReadPin(DHT22_PORT, DHT22_PIN))) Response = 1;
    }

    pMillis = HAL_GetTick();
    cMillis = HAL_GetTick();
    while ((HAL_GPIO_ReadPin(DHT22_PORT, DHT22_PIN)) && pMillis + 2 > cMillis)
    {
        cMillis = HAL_GetTick();
    }

    return Response;
}

/* ── Read 1 byte from sensor ─────────────────────────────────────────────── */
uint8_t DHT22_Read(void)
{
    uint8_t x, y = 0;

    for (x = 0; x < 8; x++)
    {
        pMillis = HAL_GetTick();
        cMillis = HAL_GetTick();
        while (!(HAL_GPIO_ReadPin(DHT22_PORT, DHT22_PIN)) && pMillis + 2 > cMillis)
        {
            cMillis = HAL_GetTick();
        }

        microDelay(40);

        if (!(HAL_GPIO_ReadPin(DHT22_PORT, DHT22_PIN)))
            y &= ~(1 << (7 - x));  // bit = 0
        else
            y |= (1 << (7 - x));   // bit = 1

        pMillis = HAL_GetTick();
        cMillis = HAL_GetTick();
        while ((HAL_GPIO_ReadPin(DHT22_PORT, DHT22_PIN)) && pMillis + 2 > cMillis)
        {
            cMillis = HAL_GetTick();
        }
    }

    return y;
}

/* ── Read all 5 bytes, verify checksum, return structured data ───────────── */
DHT22_Data_t DHT22_GetData(void)
{
    DHT22_Data_t result = {0};

    if (!DHT22_Start()) return result;  // sensor not responding, valid = 0

    uint8_t hum1   = DHT22_Read();
    uint8_t hum2   = DHT22_Read();
    uint8_t tempC1 = DHT22_Read();
    uint8_t tempC2 = DHT22_Read();
    uint8_t SUM    = DHT22_Read();

    uint8_t CHECK = hum1 + hum2 + tempC1 + tempC2;
    if (CHECK != SUM) return result;    // checksum failed, valid = 0

    // Temperature — bit 15 of tempC1 is sign bit for negatives
    if (tempC1 > 127)
        result.temperature_C = (float)tempC2 / 10.0f * (-1);
    else
        result.temperature_C = (float)((tempC1 << 8) | tempC2) / 10.0f;

    result.temperature_F = result.temperature_C * 9.0f / 5.0f + 32.0f;
    result.humidity      = (float)((hum1 << 8) | hum2) / 10.0f;
    result.valid         = 1;

    return result;
}

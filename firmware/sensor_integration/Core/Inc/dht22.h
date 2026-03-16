#ifndef DHT22_H
#define DHT22_H

#include "main.h"
#include <stdint.h>

/* ── Pin Configuration ───────────────────────────────────────────────────── */
#define DHT22_PORT      GPIOA   // GPIOA
#define DHT22_PIN       GPIO_PIN_15    // GPIO_PIN_15

/* ── Timer for µs delays (TIM3, PSC=79, ARR=65535 → 1µs/tick) ──────────── */
extern TIM_HandleTypeDef htim3;   // declared in main.c
#define DHT22_TIMER      htim3  // htim3

/* ── Data struct ─────────────────────────────────────────────────────────── */
typedef struct {
    float temperature_C;    // °C
    float temperature_F;    // °F
    float humidity;         // %RH
    uint8_t valid;          // 1 = checksum passed, 0 = read error
} DHT22_Data_t;

/* ── Function prototypes ─────────────────────────────────────────────────── */
void         DHT22_Init(void);
uint8_t      DHT22_Start(void);
uint8_t      DHT22_Read(void);
DHT22_Data_t DHT22_GetData(void);

#endif /* DHT22_H */

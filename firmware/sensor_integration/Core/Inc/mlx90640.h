/**
 ******************************************************************************
 * @file    mlx90640.h
 * @brief   MLX90640 thermal camera driver header
 ******************************************************************************
 */

#ifndef MLX90640_H
#define MLX90640_H

#include "stm32l4xx_hal.h"  // adjust for your STM32 family
#include <stdint.h>
#include <math.h>
#include <string.h>

/* ── User config ──────────────────────────────────────────────────────────── */
/* Pass the I2C handle you want the driver to use at init time.
   Internally the driver stores it in thermal_i2c.                            */

/* ── Constants ────────────────────────────────────────────────────────────── */
#define SCALEALPHA          0.000001f
#define OPENAIR_TA_SHIFT    8           ///< Default 8 °C offset from ambient
#define MLX90640_I2C_ADDR   (0x33 << 1)
#define MLX_FRAME_WORDS     832

/* ── Parameter struct ─────────────────────────────────────────────────────── */
typedef struct
{
    int16_t  kVdd;
    int16_t  vdd25;
    float    KvPTAT;
    float    KtPTAT;
    uint16_t vPTAT25;
    float    alphaPTAT;
    int16_t  gainEE;
    float    tgc;
    float    cpKv;
    float    cpKta;
    uint8_t  resolutionEE;
    uint8_t  calibrationModeEE;
    float    KsTa;
    float    ksTo[5];
    int16_t  ct[5];
    uint16_t alpha[768];
    uint8_t  alphaScale;
    int16_t  offset[768];
    int8_t   kta[768];
    uint8_t  ktaScale;
    int8_t   kv[768];
    uint8_t  kvScale;
    float    cpAlpha[2];
    int16_t  cpOffset[2];
    float    ilChessC[3];
    uint16_t brokenPixels[5];
    uint16_t outlierPixels[5];
} paramsMLX90640;

/* ── Public data ──────────────────────────────────────────────────────────── */
extern paramsMLX90640 mlx90640;
extern float     tempBuf[24 * 32];
extern uint16_t  frameBuf[24 * 32];
extern uint16_t  frameData[834];
extern uint16_t  eepromData[832];

/* ── Public API ───────────────────────────────────────────────────────────── */

/**
 * @brief  Initialise the driver with the I2C handle to use.
 *         Must be called before any other MLX90640 function.
 */
void MLX90640_SetI2C(I2C_HandleTypeDef *hi2c);

/* Low-level I2C helpers */
HAL_StatusTypeDef MLX_ReadReg  (uint16_t reg, uint16_t *value);
HAL_StatusTypeDef MLX_WriteReg (uint16_t reg, uint16_t value);
HAL_StatusTypeDef MLX_ReadBlock(uint16_t startReg, uint16_t words, uint16_t *dest);

/* Calibration extraction */
int  MLX90640_ExtractParameters(uint16_t *eeData);

/* Frame acquisition */
int   MLX90640_GetFrameData(void);
float MLX90640_GetVdd(void);
float MLX90640_GetTa(void);
void  MLX90640_CalculateTo(float emissivity, float tr, float *result);
int   MLX90640_getFrame(void);
int   MLX90640_getRawFrame(void);

/* Sensor setup */
void  MLX90640_InitSensor(uint8_t debug);

/* Diagnostics / debug */
void  MLX90640_diagnostic_test(void);
void  MLX90640_Dump_EEPROM(void);

/* float16 utilities (exposed in case the application needs them) */
uint16_t f32_to_f16(float f);
float    f16_to_f32(uint16_t h);

#endif /* MLX90640_H */

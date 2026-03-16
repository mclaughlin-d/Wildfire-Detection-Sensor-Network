#ifndef MLX90640_H
#define MLX90640_H

#include <stdint.h>
#include <string.h>
#include "stm32l4xx_hal.h"

extern I2C_HandleTypeDef  hi2c1;
extern UART_HandleTypeDef huart2;

#define SCALEALPHA          0.000001f
#define OPENAIR_TA_SHIFT    8
#define MLX90640_I2C_ADDR   (0x33 << 1)
#define MLX_FRAME_WORDS     832

extern float    scratchData[768];
extern uint16_t frameData[834];
extern uint16_t eepromData[832];
extern float    tempBuf[24 * 32];
extern uint16_t frameBuf[24 * 32];

HAL_StatusTypeDef MLX_ReadReg(I2C_HandleTypeDef *hi2c, uint16_t reg, uint16_t *value);
HAL_StatusTypeDef MLX_WriteReg(I2C_HandleTypeDef *hi2c, uint16_t reg, uint16_t value);
HAL_StatusTypeDef MLX_ReadBlock(I2C_HandleTypeDef *hi2c, uint16_t startReg, uint16_t words, uint16_t *dest);

void ExtractVDDParameters(uint16_t *eeData);
void ExtractPTATParameters(uint16_t *eeData);
void ExtractGainParameters(uint16_t *eeData);
void ExtractTgcParameters(uint16_t *eeData);
void ExtractResolutionParameters(uint16_t *eeData);
void ExtractKsTaParameters(uint16_t *eeData);
void ExtractKsToParameters(uint16_t *eeData);
void ExtractCPParameters(uint16_t *eeData);
void ExtractAlphaParameters(uint16_t *eeData);
void ExtractOffsetParameters(uint16_t *eeData);
void ExtractKtaPixelParameters(uint16_t *eeData);
void ExtractKvPixelParameters(uint16_t *eeData);
void ExtractCILCParameters(uint16_t *eeData);

int CheckAdjacentPixels(uint16_t pix1, uint16_t pix2);
int ExtractDeviatingPixels(uint16_t *eeData);
int MLX90640_ExtractParameters(uint16_t *eeData);

int   MLX90640_GetFrameData(I2C_HandleTypeDef *hi2c1);
float MLX90640_GetVdd(void);
float MLX90640_GetTa(void);
void  MLX90640_CalculateTo(float emissivity, float tr, float *result);
int   MLX90640_getFrame(I2C_HandleTypeDef *hi2c1, uint16_t* destBuf);
//int   MLX90640_getRawFrame(void);

void MLX90640_InitSensor(I2C_HandleTypeDef *hi2c1, uint8_t debug);
void MLX90640_diagnostic_test(I2C_HandleTypeDef *hi2c1);
void MLX90640_Dump_EEPROM(void);

#endif

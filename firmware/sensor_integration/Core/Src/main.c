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
#include "stdio.h"
#include "string.h"
#include <math.h>

#include "dht22.h"
//#include "mlx90640.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* ── UART to Computer / Print to Console ────────────────────────────────── */
#define COM_UART &huart2

/* ── GPS Module NEO-6M (UART4) ───────────────────────────────────────────── */
#define GPS_UART                &huart4
#define GPS_TX_PORT             GPIOC
#define GPS_TX_PIN              GPIO_PIN_10   // PC10 → RX of NEO-6M
#define GPS_RX_PORT             GPIOC
#define GPS_RX_PIN              GPIO_PIN_11   // PC11 → TX of NEO-6M

/* ── Servo (TIM1_CH1) ────────────────────────────────────────────────────── */
#define SERVO_TIMER             &htim1
#define SERVO_CHANNEL           TIM_CHANNEL_1

#define SERVO_0_DEG    500   // 0.5ms pulse
#define SERVO_90_DEG   1500   // 1.5ms pulse
#define SERVO_180_DEG  2500   // 2.5ms pulse
#define SERVO_DELAY_MS 2000   // 2 seconds between positions

/* ── Gas Sensor MQ-2 (ADC1_IN4) ─────────────────────────────────────────── */
#define GAS_SENSOR_PORT         GPIOC
#define GAS_SENSOR_PIN          GPIO_PIN_3
#define GAS_SENSOR_ADC          &hadc1

/* ── Thermal Camera (I2C1) ───────────────────────────────────────────────── */
#define THERMAL_I2C             &hi2c1
#define THERMAL_SCL_PORT        GPIOB
#define THERMAL_SCL_PIN         GPIO_PIN_6    // PB6 — needs 10kΩ pull-up to 3.3V
#define THERMAL_SDA_PORT        GPIOB
#define THERMAL_SDA_PIN         GPIO_PIN_7    // PB7 — needs 10kΩ pull-up to 3.3V

/* ── Fans (TIM2_CH3, TIM2_CH4) ──────────────────────────────────────────── */
#define FAN_TIMER               &htim2
#define FAN1_CHANNEL            TIM_CHANNEL_3
#define FAN2_CHANNEL            TIM_CHANNEL_4

#define FAN_ON					30			  // 30% duty cycle
#define FAN_OFF					0			  // 0% duty cycle



/* ── TIM1 PWM Settings (Servo) ───────────────────────────────────────────── */
#define SERVO_PWM_PRESCALER     79            // 80MHz / 80 = 1MHz
#define SERVO_PWM_PERIOD        19999         // 1MHz / 20000 = 50Hz

/* ── TIM2 PWM Settings (Fans) ────────────────────────────────────────────── */
#define FAN_PWM_PRESCALER       39            // 80MHz / 40 = 2MHz
#define FAN_PWM_PERIOD          100           // 2MHz / 101 = ~19.8kHz


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim3;

UART_HandleTypeDef huart4;
UART_HandleTypeDef huart5;
UART_HandleTypeDef huart2;
DMA_HandleTypeDef hdma_uart4_rx;

/* USER CODE BEGIN PV */
uint16_t adc_value = 0;
char buff[50];
uint8_t msgPayload[214];

uint8_t flag = 0;

// this interrupts changes flag to 1 as soon as the uint8_t buff[300] is full
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {

	flag = 1;

}

uint16_t frameDataBuf[32 * 24];

struct msgPayload {
		uint8_t moduleID;
		uint8_t row;
		uint16_t hr;
		uint16_t min;
		uint16_t sec;
		uint16_t gas;
		float temp;
		float hum;
		float volt;
		uint16_t pixels[96];
	} __packed;


	/* --- float32 <-> float16 helpers --- */



/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM1_Init(void);
static void MX_UART4_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM3_Init(void);
static void MX_I2C1_Init(void);
static void MX_UART5_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

#define SCALEALPHA 0.000001
#define OPENAIR_TA_SHIFT 0 // 8 ///< Default 8 degree offset from ambient air
#define MLX90640_I2C_ADDR 0x33<<1
#define MLX_FRAME_WORDS 832

/* --- float32 <-> float16 helpers --- */
static uint16_t f32_to_f16(float f)
{
    uint32_t x;
    memcpy(&x, &f, sizeof(x));

    uint16_t sign    = (x >> 16) & 0x8000;
    int32_t  exp     = ((x >> 23) & 0xFF) - 127 + 15;
    uint32_t mant    = x & 0x007FFFFF;

    if (exp <= 0) {
        /* Flush to signed zero (subnormals not worth the cost on MCU) */
        return sign;
    } else if (exp >= 31) {
        /* Overflow -> infinity */
        return sign | 0x7C00;
    }
    return sign | ((uint16_t)exp << 10) | (uint16_t)(mant >> 13);
}

static float f16_to_f32(uint16_t h)
{
    uint32_t sign = (uint32_t)(h & 0x8000) << 16;
    uint32_t exp  = (h >> 10) & 0x1F;
    uint32_t mant = h & 0x03FF;
    uint32_t x;

    if (exp == 0) {
        x = sign; /* zero / subnormal -> zero */
    } else if (exp == 31) {
        x = sign | 0x7F800000 | (mant << 13); /* inf / NaN */
    } else {
        x = sign | ((exp - 15 + 127) << 23) | (mant << 13);
    }

    float f;
    memcpy(&f, &x, sizeof(f));
    return f;
}

struct paramsMLX90640 {
  int16_t kVdd;
  int16_t vdd25;
  float KvPTAT;
  float KtPTAT;
  uint16_t vPTAT25;
  float alphaPTAT;
  int16_t gainEE;
  float tgc;
  float cpKv;
  float cpKta;
  uint8_t resolutionEE;
  uint8_t calibrationModeEE;
  float KsTa;
  float ksTo[5];
  int16_t ct[5];
   uint16_t alpha[768];
  uint8_t alphaScale;
   int16_t offset[768];
  int8_t kta[768];
  uint8_t ktaScale;
   int8_t kv[768];
  uint8_t kvScale;
  float cpAlpha[2];
   int16_t cpOffset[2];
  float ilChessC[3];
  uint16_t brokenPixels[5];
  uint16_t outlierPixels[5];
};

struct paramsMLX90640 mlx90640 = { 0 };

float scratchData[768]; // for calibration parameter calculations
uint16_t frameData[834] = { 0 }; // for reading frame data from sensor
uint8_t rawReadBuf[834 * 2] = { 0 };
uint16_t eepromData[832]; // for storing eeprom data
float tempBuf[24 * 32] = { 0 }; // for temp calculation
uint16_t frameBuf[24 * 32] = { 0 }; // for final float temperature values for each frame
float tempBufAgain[24 * 32] = { 0 };


// various messages for sending over UART
uint8_t Buffer[25] = {0};
uint8_t Space[] = " - ";
uint8_t StartMSG[] = "Starting I2C Scanning: \r\n";
uint8_t EndMSG[] = "Done! \r\n\r\n";
uint8_t StartFrameMsg[] = "\nStarting to read frame: \n";
uint8_t EndFrameMsg[] = "\nFinished frame read!\n";
uint8_t IsFrameReadyMsg[] = "\nFrame data ready\n";
uint8_t ModeSetMsg[] = "\nSet chess mode\n";
uint8_t ResolutionSetMsg[] = "\nSet resolution\n";
uint8_t FrameRateSetMsg[] = "\nSet frame rate\n";
uint8_t ErrorMsgRead[] = "\nError in read\n";
uint8_t ErrorMsgWrite[] = "\nError in write\n";
uint8_t ChessMsg[] = "\nCHESS\n";
uint8_t CrashMsg[] = "\n\nCRASH\n\n";

HAL_StatusTypeDef MLX_ReadReg(I2C_HandleTypeDef *hi2c,
                               uint16_t reg,
                               uint16_t *value)
{
    uint8_t buffer[2];

    HAL_StatusTypeDef status =
        HAL_I2C_Mem_Read(hi2c,
                         MLX90640_I2C_ADDR,
                         reg,
                         I2C_MEMADD_SIZE_16BIT,
                         buffer,
                         2,
                         1000);

    if (status != HAL_OK)
        return status;

    *value = (buffer[0] << 8) | buffer[1];  // MLX = MSB first
    return HAL_OK;
}


HAL_StatusTypeDef MLX_WriteReg(I2C_HandleTypeDef *hi2c,
                                uint16_t reg,
                                uint16_t value)
{
    uint8_t buffer[2];

    buffer[0] = value >> 8;      // MSB
    buffer[1] = value & 0xFF;    // LSB

    return HAL_I2C_Mem_Write(hi2c,
                             MLX90640_I2C_ADDR,
                             reg,
                             I2C_MEMADD_SIZE_16BIT,
                             buffer,
                             2,
                             1000);
}

HAL_StatusTypeDef MLX_ReadBlock(I2C_HandleTypeDef *hi2c,
                                       uint16_t startReg,
                                       uint16_t words,
                                       uint16_t *dest)
{
    // uint8_t raw[64];   // small buffer
    uint8_t raw[512];
    uint16_t remaining = words;
    uint16_t offset = 0;

    while (remaining)
    {
//        uint16_t chunkWords = (remaining > 32) ? 32 : remaining;
        uint16_t chunkWords = (remaining > 256) ? 256 : remaining;
        uint16_t chunkBytes = chunkWords * 2;

        HAL_StatusTypeDef status =
            HAL_I2C_Mem_Read(hi2c,
                             MLX90640_I2C_ADDR,
                             startReg + offset,
                             I2C_MEMADD_SIZE_16BIT,
                             raw,
                             chunkBytes,
                             2000);

        if (status != HAL_OK)
            return status;

        for (uint16_t i = 0; i < chunkWords; i++)
        {
            dest[offset + i] =
                (raw[2*i] << 8) | raw[2*i + 1];
        }

        offset += chunkWords;
        remaining -= chunkWords;
    }

    return HAL_OK;
}


void ExtractVDDParameters(uint16_t *eeData)
{
    int16_t kVdd;
    int16_t vdd25;

    kVdd = eeData[51];

    kVdd = (eeData[51] & 0xFF00) >> 8;
    if(kVdd > 127)
    {
        kVdd = kVdd - 256;
    }
    kVdd = 32 * kVdd;
    vdd25 = eeData[51] & 0x00FF;
    vdd25 = ((vdd25 - 256) << 5) - 8192;

    mlx90640.kVdd = kVdd;
    mlx90640.vdd25 = vdd25;
}


void ExtractPTATParameters(uint16_t *eeData)
{
    float KvPTAT;
    float KtPTAT;
    int16_t vPTAT25;
    float alphaPTAT;


    KvPTAT = (eeData[50] & 0xFC00) >> 10;
    if(KvPTAT > 31)
    {
        KvPTAT = KvPTAT - 64;
    }
    KvPTAT = KvPTAT/4096;

    KtPTAT = eeData[50] & 0x03FF;
    if(KtPTAT > 511)
    {
        KtPTAT = KtPTAT - 1024;
    }
    KtPTAT = KtPTAT/8;

    vPTAT25 = eeData[49];

    alphaPTAT = (eeData[16] & 0xF000) / pow(2, (double)14) + 8.0f;

    mlx90640.KvPTAT = KvPTAT;
    mlx90640.KtPTAT = KtPTAT;
    mlx90640.vPTAT25 = vPTAT25;
    mlx90640.alphaPTAT = alphaPTAT;
}

void ExtractGainParameters(uint16_t *eeData)
{
    int16_t gainEE;

    gainEE = eeData[48];
    if(gainEE > 32767)
    {
        gainEE = gainEE -65536;
    }

    mlx90640.gainEE = gainEE;
}

void ExtractTgcParameters(uint16_t *eeData)
{
    float tgc;
    tgc = eeData[60] & 0x00FF;
    if(tgc > 127)
    {
        tgc = tgc - 256;
    }
    tgc = tgc / 32.0f;

    mlx90640.tgc = tgc;
}

void ExtractResolutionParameters(uint16_t *eeData)
{
    uint8_t resolutionEE;
    resolutionEE = (eeData[56] & 0x3000) >> 12;

    mlx90640.resolutionEE = resolutionEE;
}

void ExtractKsTaParameters(uint16_t *eeData)
{
    float KsTa;
    KsTa = (eeData[60] & 0xFF00) >> 8;
    if(KsTa > 127)
    {
        KsTa = KsTa -256;
    }
    KsTa = KsTa / 8192.0f;

    mlx90640.KsTa = KsTa;
}

void ExtractKsToParameters(uint16_t *eeData)
{
    int KsToScale;
    int8_t step;

    step = ((eeData[63] & 0x3000) >> 12) * 10;

    mlx90640.ct[0] = -40;
    mlx90640.ct[1] = 0;
    mlx90640.ct[2] = (eeData[63] & 0x00F0) >> 4;
    mlx90640.ct[3] = (eeData[63] & 0x0F00) >> 8;

    mlx90640.ct[2] = mlx90640.ct[2]*step;
    mlx90640.ct[3] = mlx90640.ct[2] + mlx90640.ct[3]*step;
    mlx90640.ct[4] = 400;

    KsToScale = (eeData[63] & 0x000F) + 8;
    KsToScale = 1 << KsToScale;

    mlx90640.ksTo[0] = eeData[61] & 0x00FF;
    mlx90640.ksTo[1] = (eeData[61] & 0xFF00) >> 8;
    mlx90640.ksTo[2] = eeData[62] & 0x00FF;
    mlx90640.ksTo[3] = (eeData[62] & 0xFF00) >> 8;

    for(int i = 0; i < 4; i++)
    {
        if(mlx90640.ksTo[i] > 127)
        {
            mlx90640.ksTo[i] = mlx90640.ksTo[i] - 256;
        }
        mlx90640.ksTo[i] = mlx90640.ksTo[i] / KsToScale;
    }

    mlx90640.ksTo[4] = -0.0002;
}


void ExtractCPParameters(uint16_t *eeData)
{
    float alphaSP[2];
    int16_t offsetSP[2];
    float cpKv;
    float cpKta;
    uint8_t alphaScale;
    uint8_t ktaScale1;
    uint8_t kvScale;

    alphaScale = ((eeData[32] & 0xF000) >> 12) + 27;

    offsetSP[0] = (eeData[58] & 0x03FF);
    if (offsetSP[0] > 511)
    {
        offsetSP[0] = offsetSP[0] - 1024;
    }

    offsetSP[1] = (eeData[58] & 0xFC00) >> 10;
    if (offsetSP[1] > 31)
    {
        offsetSP[1] = offsetSP[1] - 64;
    }
    offsetSP[1] = offsetSP[1] + offsetSP[0];

    alphaSP[0] = (eeData[57] & 0x03FF);
    if (alphaSP[0] > 511)
    {
        alphaSP[0] = alphaSP[0] - 1024;
    }
    alphaSP[0] = alphaSP[0] /  pow(2,(double)alphaScale);

    alphaSP[1] = (eeData[57] & 0xFC00) >> 10;
    if (alphaSP[1] > 31)
    {
        alphaSP[1] = alphaSP[1] - 64;
    }
    alphaSP[1] = (1 + alphaSP[1]/128) * alphaSP[0];

    cpKta = (eeData[59] & 0x00FF);
    if (cpKta > 127)
    {
        cpKta = cpKta - 256;
    }
    ktaScale1 = ((eeData[56] & 0x00F0) >> 4) + 8;
    mlx90640.cpKta = cpKta / pow(2,(double)ktaScale1);

    cpKv = (eeData[59] & 0xFF00) >> 8;
    if (cpKv > 127)
    {
        cpKv = cpKv - 256;
    }
    kvScale = (eeData[56] & 0x0F00) >> 8;
    mlx90640.cpKv = cpKv / pow(2,(double)kvScale);

    mlx90640.cpAlpha[0] = alphaSP[0];
    mlx90640.cpAlpha[1] = alphaSP[1];
    mlx90640.cpOffset[0] = offsetSP[0];
    mlx90640.cpOffset[1] = offsetSP[1];
}


void ExtractAlphaParameters(uint16_t *eeData)
{
    int16_t accRow[24];
    int16_t accColumn[32];
    int p = 0;
    int alphaRef;
    uint8_t alphaScale;
    uint8_t accRowScale;
    uint8_t accColumnScale;
    uint8_t accRemScale;
    float temp;


    accRemScale = eeData[32] & 0x000F;
    accColumnScale = (eeData[32] & 0x00F0) >> 4;
    accRowScale = (eeData[32] & 0x0F00) >> 8;
    alphaScale = ((eeData[32] & 0xF000) >> 12) + 30;
    alphaRef = eeData[33];

    for(int i = 0; i < 6; i++)
    {
        p = i * 4;
        accRow[p + 0] = (eeData[34 + i] & 0x000F);
        accRow[p + 1] = (eeData[34 + i] & 0x00F0) >> 4;
        accRow[p + 2] = (eeData[34 + i] & 0x0F00) >> 8;
        accRow[p + 3] = (eeData[34 + i] & 0xF000) >> 12;
    }

    for(int i = 0; i < 24; i++)
    {
        if (accRow[i] > 7)
        {
            accRow[i] = accRow[i] - 16;
        }
    }

    for(int i = 0; i < 8; i++)
    {
        p = i * 4;
        accColumn[p + 0] = (eeData[40 + i] & 0x000F);
        accColumn[p + 1] = (eeData[40 + i] & 0x00F0) >> 4;
        accColumn[p + 2] = (eeData[40 + i] & 0x0F00) >> 8;
        accColumn[p + 3] = (eeData[40 + i] & 0xF000) >> 12;
    }

    for(int i = 0; i < 32; i ++)
    {
        if (accColumn[i] > 7)
        {
            accColumn[i] = accColumn[i] - 16;
        }
    }

    for(int i = 0; i < 24; i++)
    {
        for(int j = 0; j < 32; j ++)
        {
            p = 32 * i +j;
            scratchData[p] = (eeData[64 + p] & 0x03F0) >> 4;
            if (scratchData[p] > 31)
            {
                scratchData[p] = scratchData[p] - 64;
            }
            scratchData[p] = scratchData[p]*(1 << accRemScale);
            scratchData[p] = (alphaRef + (accRow[i] << accRowScale) + (accColumn[j] << accColumnScale) + scratchData[p]);
            scratchData[p] = scratchData[p] / pow(2,(double)alphaScale);
            scratchData[p] = scratchData[p] - mlx90640.tgc * (mlx90640.cpAlpha[0] + mlx90640.cpAlpha[1])/2;
            scratchData[p] = SCALEALPHA/scratchData[p];

            if (i == 0 && j == 0) temp = scratchData[p];
            else {
                if (scratchData[p] > temp) temp = scratchData[p];
            }
        }
    }

     temp = scratchData[0];
     for(int i = 1; i < 768; i++)
     {
         if (scratchData[i] > temp)
         {
             temp = scratchData[i];
         }
     }

    alphaScale = 0;
    if (temp <= 0) temp = 1;  // prevent lock
    while(temp < 32768)
    {
        temp = temp*2;
        alphaScale = alphaScale + 1;
    }

    for(int i = 0; i < 768; i++)
    {
        temp = scratchData[i] * pow(2,(double)alphaScale);
        mlx90640.alpha[i] = (temp + 0.5);
    }

    mlx90640.alphaScale = alphaScale;
}

void ExtractOffsetParameters(uint16_t *eeData)
{
    int16_t occRow[24];
    int16_t occColumn[32];
    int p = 0;
    int16_t offsetRef;
    uint8_t occRowScale;
    uint8_t occColumnScale;
    uint8_t occRemScale;


    occRemScale = (eeData[16] & 0x000F);
    occColumnScale = (eeData[16] & 0x00F0) >> 4;
    occRowScale = (eeData[16] & 0x0F00) >> 8;
    offsetRef = eeData[17];
    if (offsetRef > 32767)
    {
        offsetRef = offsetRef - 65536;
    }

    for(int i = 0; i < 6; i++)
    {
        p = i * 4;
        occRow[p + 0] = (eeData[18 + i] & 0x000F);
        occRow[p + 1] = (eeData[18 + i] & 0x00F0) >> 4;
        occRow[p + 2] = (eeData[18 + i] & 0x0F00) >> 8;
        occRow[p + 3] = (eeData[18 + i] & 0xF000) >> 12;
    }

    for(int i = 0; i < 24; i++)
    {
        if (occRow[i] > 7)
        {
            occRow[i] = occRow[i] - 16;
        }
    }

    for(int i = 0; i < 8; i++)
    {
        p = i * 4;
        occColumn[p + 0] = (eeData[24 + i] & 0x000F);
        occColumn[p + 1] = (eeData[24 + i] & 0x00F0) >> 4;
        occColumn[p + 2] = (eeData[24 + i] & 0x0F00) >> 8;
        occColumn[p + 3] = (eeData[24 + i] & 0xF000) >> 12;
    }

    for(int i = 0; i < 32; i ++)
    {
        if (occColumn[i] > 7)
        {
            occColumn[i] = occColumn[i] - 16;
        }
    }

    for(int i = 0; i < 24; i++)
    {
        for(int j = 0; j < 32; j ++)
        {
            p = 32 * i +j;
            mlx90640.offset[p] = (eeData[64 + p] & 0xFC00) >> 10;
            if (mlx90640.offset[p] > 31)
            {
                mlx90640.offset[p] = mlx90640.offset[p] - 64;
            }
            mlx90640.offset[p] = mlx90640.offset[p]*(1 << occRemScale);
            mlx90640.offset[p] = (offsetRef + (occRow[i] << occRowScale) + (occColumn[j] << occColumnScale) + mlx90640.offset[p]);
        }
    }
}


void ExtractKtaPixelParameters(uint16_t *eeData)
{
    int p = 0;
    int8_t KtaRC[4];
    int8_t KtaRoCo;
    int8_t KtaRoCe;
    int8_t KtaReCo;
    int8_t KtaReCe;
    uint8_t ktaScale1;
    uint8_t ktaScale2;
    uint8_t split;
    float temp;

    KtaRoCo = (eeData[54] & 0xFF00) >> 8;
    if (KtaRoCo > 127)
    {
        KtaRoCo = KtaRoCo - 256;
    }
    KtaRC[0] = KtaRoCo;

    KtaReCo = (eeData[54] & 0x00FF);
    if (KtaReCo > 127)
    {
        KtaReCo = KtaReCo - 256;
    }
    KtaRC[2] = KtaReCo;

    KtaRoCe = (eeData[55] & 0xFF00) >> 8;
    if (KtaRoCe > 127)
    {
        KtaRoCe = KtaRoCe - 256;
    }
    KtaRC[1] = KtaRoCe;

    KtaReCe = (eeData[55] & 0x00FF);
    if (KtaReCe > 127)
    {
        KtaReCe = KtaReCe - 256;
    }
    KtaRC[3] = KtaReCe;

    ktaScale1 = ((eeData[56] & 0x00F0) >> 4) + 8;
    ktaScale2 = (eeData[56] & 0x000F);

    for(int i = 0; i < 24; i++)
    {
        for(int j = 0; j < 32; j ++)
        {
            p = 32 * i +j;
            split = 2*(p/32 - (p/64)*2) + p%2;
            scratchData[p] = (eeData[64 + p] & 0x000E) >> 1;
            if (scratchData[p] > 3)
            {
                scratchData[p] = scratchData[p] - 8;
            }
            scratchData[p] = scratchData[p] * (1 << ktaScale2);
            scratchData[p] = KtaRC[split] + scratchData[p];
            scratchData[p] = scratchData[p] / pow(2,(double)ktaScale1);
            scratchData[p] = scratchData[p] * mlx90640.offset[p];

            if (i == 0 && j == 0) temp = fabs(scratchData[p]);
            else {
                if (fabs(scratchData[p]) > temp) temp = fabs(scratchData[p]);
            }
        }
    }

     temp = fabs(scratchData[0]);
     for(int i = 1; i < 768; i++)
     {
         if (fabs(scratchData[i]) > temp)
         {
             temp = fabs(scratchData[i]);
         }
     }

    ktaScale1 = 0;
    if (temp == 0) temp = 1;
    while(temp < 64)
    {
        temp = temp*2;
        ktaScale1 = ktaScale1 + 1;
    }

    for(int i = 0; i < 768; i++)
    {
        temp = scratchData[i] * pow(2,(double)ktaScale1);
        if (temp < 0)
        {
            mlx90640.kta[i] = (temp - 0.5);
        }
        else
        {
            mlx90640.kta[i] = (temp + 0.5);
        }

    }

    mlx90640.ktaScale = ktaScale1;
}

void ExtractKvPixelParameters(uint16_t *eeData)
{
    int p = 0;
    int8_t KvT[4];
    int8_t KvRoCo;
    int8_t KvRoCe;
    int8_t KvReCo;
    int8_t KvReCe;
    uint8_t kvScale;
    uint8_t split;
    float temp;

    KvRoCo = (eeData[52] & 0xF000) >> 12;
    if (KvRoCo > 7)
    {
        KvRoCo = KvRoCo - 16;
    }
    KvT[0] = KvRoCo;

    KvReCo = (eeData[52] & 0x0F00) >> 8;
    if (KvReCo > 7)
    {
        KvReCo = KvReCo - 16;
    }
    KvT[2] = KvReCo;

    KvRoCe = (eeData[52] & 0x00F0) >> 4;
    if (KvRoCe > 7)
    {
        KvRoCe = KvRoCe - 16;
    }
    KvT[1] = KvRoCe;

    KvReCe = (eeData[52] & 0x000F);
    if (KvReCe > 7)
    {
        KvReCe = KvReCe - 16;
    }
    KvT[3] = KvReCe;

    kvScale = (eeData[56] & 0x0F00) >> 8;


    for(int i = 0; i < 24; i++)
    {
        for(int j = 0; j < 32; j ++)
        {
            p = 32 * i +j;
            split = 2*(p/32 - (p/64)*2) + p%2;
            scratchData[p] = KvT[split];
            scratchData[p] = scratchData[p] / pow(2,(double)kvScale);
            scratchData[p] = scratchData[p] * mlx90640.offset[p];

            if (i == 0 && j == 0) temp = fabs(scratchData[p]);
            else {
                if (fabs(scratchData[p]) > temp) temp = fabs(scratchData[p]);
            }
        }
    }

     temp = fabs(scratchData[0]);
     for(int i = 1; i < 768; i++)
     {
         if (fabs(scratchData[i]) > temp)
         {
             temp = fabs(scratchData[i]);
         }
     }

    kvScale = 0;

    if (temp == 0) temp = 1;
    while(temp < 64)
    {
        temp = temp*2;
        kvScale = kvScale + 1;
    }

    for(int i = 0; i < 768; i++)
    {
        temp = scratchData[i] * pow(2,(double)kvScale);
        if (temp < 0)
        {
            mlx90640.kv[i] = (temp - 0.5);
        }
        else
        {
            mlx90640.kv[i] = (temp + 0.5);
        }

    }

    mlx90640.kvScale = kvScale;
}


void ExtractCILCParameters(uint16_t *eeData)
{
    float ilChessC[3];
    uint8_t calibrationModeEE;

    calibrationModeEE = (eeData[10] & 0x0800) >> 4;
    calibrationModeEE = calibrationModeEE ^ 0x80;

    ilChessC[0] = (eeData[53] & 0x003F);
    if (ilChessC[0] > 31)
    {
        ilChessC[0] = ilChessC[0] - 64;
    }
    ilChessC[0] = ilChessC[0] / 16.0f;

    ilChessC[1] = (eeData[53] & 0x07C0) >> 6;
    if (ilChessC[1] > 15)
    {
        ilChessC[1] = ilChessC[1] - 32;
    }
    ilChessC[1] = ilChessC[1] / 2.0f;

    ilChessC[2] = (eeData[53] & 0xF800) >> 11;
    if (ilChessC[2] > 15)
    {
        ilChessC[2] = ilChessC[2] - 32;
    }
    ilChessC[2] = ilChessC[2] / 8.0f;

    mlx90640.calibrationModeEE = calibrationModeEE;
    mlx90640.ilChessC[0] = ilChessC[0];
    mlx90640.ilChessC[1] = ilChessC[1];
    mlx90640.ilChessC[2] = ilChessC[2];
}

int CheckAdjacentPixels(uint16_t pix1, uint16_t pix2)
{
    int pixPosDif;

    pixPosDif = pix1 - pix2;
    if(pixPosDif > -34 && pixPosDif < -30)
    {
        return -6;
    }
    if(pixPosDif > -2 && pixPosDif < 2)
    {
        return -6;
    }
    if(pixPosDif > 30 && pixPosDif < 34)
    {
        return -6;
    }

    return 0;
}

int ExtractDeviatingPixels(uint16_t *eeData)
{
    uint16_t pixCnt = 0;
    uint16_t brokenPixCnt = 0;
    uint16_t outlierPixCnt = 0;
    int warn = 0;
    int i;

    for(pixCnt = 0; pixCnt<5; pixCnt++)
    {
        mlx90640.brokenPixels[pixCnt] = 0xFFFF;
        mlx90640.outlierPixels[pixCnt] = 0xFFFF;
    }

    pixCnt = 0;
    while (pixCnt < 768 && brokenPixCnt < 5 && outlierPixCnt < 5)
    {
        if(eeData[pixCnt+64] == 0)
        {
            mlx90640.brokenPixels[brokenPixCnt] = pixCnt;
            brokenPixCnt = brokenPixCnt + 1;
        }
        else if((eeData[pixCnt+64] & 0x0001) != 0)
        {
            mlx90640.outlierPixels[outlierPixCnt] = pixCnt;
            outlierPixCnt = outlierPixCnt + 1;
        }

        pixCnt = pixCnt + 1;

    }

    if(brokenPixCnt > 4)
    {
        warn = -3;
    }
    else if(outlierPixCnt > 4)
    {
        warn = -4;
    }
    else if((brokenPixCnt + outlierPixCnt) > 4)
    {
        warn = -5;
    }
    else
    {
        for(pixCnt=0; pixCnt<brokenPixCnt; pixCnt++)
        {
            for(i=pixCnt+1; i<brokenPixCnt; i++)
            {
                warn = CheckAdjacentPixels(mlx90640.brokenPixels[pixCnt],mlx90640.brokenPixels[i]);
                if(warn != 0)
                {
                    return warn;
                }
            }
        }

        for(pixCnt=0; pixCnt<outlierPixCnt; pixCnt++)
        {
            for(i=pixCnt+1; i<outlierPixCnt; i++)
            {
                warn = CheckAdjacentPixels(mlx90640.outlierPixels[pixCnt],mlx90640.outlierPixels[i]);
                if(warn != 0)
                {
                    return warn;
                }
            }
        }

        for(pixCnt=0; pixCnt<brokenPixCnt; pixCnt++)
        {
            for(i=0; i<outlierPixCnt; i++)
            {
                warn = CheckAdjacentPixels(mlx90640.brokenPixels[pixCnt],mlx90640.outlierPixels[i]);
                if(warn != 0)
                {
                    return warn;
                }
            }
        }

    }


    return warn;

}

int MLX90640_ExtractParameters(uint16_t* eeData) {
	int error = 0;
	ExtractVDDParameters(eeData);
	ExtractPTATParameters(eeData);
	ExtractGainParameters(eeData);
	ExtractTgcParameters(eeData );
	ExtractResolutionParameters(eeData );
	ExtractKsTaParameters(eeData );
	ExtractKsToParameters(eeData );
	ExtractCPParameters(eeData );
	ExtractAlphaParameters(eeData );
	 ExtractOffsetParameters(eeData );
	ExtractKtaPixelParameters(eeData );
	 ExtractKvPixelParameters(eeData );
	ExtractCILCParameters(eeData );
	error = ExtractDeviatingPixels(eeData );

	return error;
}


int MLX90640_GetFrameData()
{
    uint16_t dataReady = 1;
    uint16_t controlRegister1;
    uint16_t statusRegister;
    uint8_t cnt = 0;

    dataReady = 0;
    while(dataReady == 0)
    {
    	if (MLX_ReadReg(&hi2c1, 0x8000, &statusRegister) != HAL_OK)
		{
			HAL_UART_Transmit(&huart2, ErrorMsgRead, sizeof(ErrorMsgRead), 10000);
			break;
		}
		dataReady = statusRegister & 0x0008;
	}

    while(dataReady != 0 && cnt < 5)
    {

        if (MLX_WriteReg(&hi2c1, 0x8000, 0x0030) != HAL_OK) {

                	HAL_UART_Transmit(&huart2, ErrorMsgWrite, sizeof(ErrorMsgWrite), 10000);
                				break;

                }

//        if (MLX_ReadBlock(&hi2c1, 0x0400, 832, frameData) != HAL_OK) {
//                	HAL_UART_Transmit(&huart2, ErrorMsgRead, sizeof(ErrorMsgRead), 10000);
//        		break;
//        	}
        HAL_StatusTypeDef status =
                HAL_I2C_Mem_Read(&hi2c1,
                                 MLX90640_I2C_ADDR,
                                 0x0400,
                                 I2C_MEMADD_SIZE_16BIT,
                                 rawReadBuf,
                                 832 * 2,
                                 2000);

        for (uint16_t i = 0; i < 834; i++)
                {
                    frameData[i] =
                        (rawReadBuf[2*i] << 8) | rawReadBuf[2*i + 1];
                }
        if (status != HAL_OK) {
                       	HAL_UART_Transmit(&huart2, ErrorMsgRead, sizeof(ErrorMsgRead), 10000);
               		break;
               	}

        if (MLX_ReadReg(&hi2c1, 0x8000, &statusRegister) != HAL_OK) {
                	HAL_UART_Transmit(&huart2, ErrorMsgRead, sizeof(ErrorMsgRead), 10000);
                				break;

                }
        dataReady = statusRegister & 0x0008;
        cnt = cnt + 1;
    }

    if(cnt > 4)
    {
    	char errorBuf[] = "Count > 4\n";
    	HAL_UART_Transmit(&huart2, errorBuf, sizeof(errorBuf), 10000);
        return -8;
    }

    if (MLX_ReadReg(&hi2c1, 0x800D, &controlRegister1) != HAL_OK) {
        	HAL_UART_Transmit(&huart2, ErrorMsgRead, sizeof(ErrorMsgRead), 10000);

        }
    frameData[832] = controlRegister1;
    frameData[833] = statusRegister & 0x0001;

    return frameData[833];
}


float MLX90640_GetVdd()
{
    float vdd;
    float resolutionCorrection;

    int resolutionRAM;

    vdd = frameData[810];
    if(vdd > 32767)
    {
        vdd = vdd - 65536;
    }
    resolutionRAM = (frameData[832] & 0x0C00) >> 10;
    resolutionCorrection = pow(2, (double)mlx90640.resolutionEE) / pow(2, (double)resolutionRAM);
    vdd = (resolutionCorrection * vdd - mlx90640.vdd25) / mlx90640.kVdd + 3.3;

    return vdd;
}

float MLX90640_GetTa()
{
    float ptat;
    float ptatArt;
    float vdd;
    float ta;

    vdd = MLX90640_GetVdd();

    char testBuf[32] = { 0 };
    sprintf(testBuf, "Voltage: %lf\n", vdd);
    HAL_UART_Transmit(&huart2, testBuf, sizeof(testBuf), 10000);

    ptat = frameData[800];
    if(ptat > 32767)
    {
        ptat = ptat - 65536;
    }

    ptatArt = frameData[768];
    if(ptatArt > 32767)
    {
        ptatArt = ptatArt - 65536;
    }
    ptatArt = (ptat / (ptat * mlx90640.alphaPTAT + ptatArt)) * pow(2, (double)18);

    ta = (ptatArt / (1 + mlx90640.KvPTAT * (vdd - 3.3)) - mlx90640.vPTAT25);
    ta = ta / mlx90640.KtPTAT + 25;

    return ta;
}

void MLX90640_CalculateTo( float emissivity, float tr, float *result)
{
    float vdd;
    float ta;
    float ta4;
    float tr4;
    float taTr;
    float gain;
    float irDataCP[2];
    float irData;
    float alphaCompensated;
    uint8_t mode;
    int8_t ilPattern;
    int8_t chessPattern;
    int8_t pattern;
    int8_t conversionPattern;
    float Sx;
    float To;
    float alphaCorrR[4];
    int8_t range;
    uint16_t subPage;
    float ktaScale;
    float kvScale;
    float alphaScale;
    float kta;
    float kv;

    subPage = frameData[833];
    vdd = MLX90640_GetVdd();
    ta = MLX90640_GetTa();

    char taBuf[32] = { 0 };
    sprintf(taBuf, "Temp: %lf\n", ta);
    HAL_UART_Transmit(&huart2, taBuf, sizeof(taBuf), 10000);

    ta4 = (ta + 273.15);
    ta4 = ta4 * ta4;
    ta4 = ta4 * ta4;
    tr4 = (tr + 273.15);
    tr4 = tr4 * tr4;
    tr4 = tr4 * tr4;
    taTr = tr4 - (tr4-ta4)/emissivity;

    ktaScale = pow(2,(double)mlx90640.ktaScale);
    kvScale = pow(2,(double)mlx90640.kvScale);
    alphaScale = pow(2,(double)mlx90640.alphaScale);

    alphaCorrR[0] = 1 / (1 + mlx90640.ksTo[0] * 40);
    alphaCorrR[1] = 1 ;
    alphaCorrR[2] = (1 + mlx90640.ksTo[1] * mlx90640.ct[2]);
    alphaCorrR[3] = alphaCorrR[2] * (1 + mlx90640.ksTo[2] * (mlx90640.ct[3] - mlx90640.ct[2]));

//------------------------- Gain calculation -----------------------------------
    gain = frameData[778];
    if(gain > 32767)
    {
        gain = gain - 65536;
    }

    gain = mlx90640.gainEE / gain;

//------------------------- To calculation -------------------------------------
    mode = (frameData[832] & 0x1000) >> 5;

    irDataCP[0] = frameData[776];
    irDataCP[1] = frameData[808];
    for( int i = 0; i < 2; i++)
    {
        if(irDataCP[i] > 32767)
        {
            irDataCP[i] = irDataCP[i] - 65536;
        }
        irDataCP[i] = irDataCP[i] * gain;
    }
    irDataCP[0] = irDataCP[0] - mlx90640.cpOffset[0] * (1 + mlx90640.cpKta * (ta - 25)) * (1 + mlx90640.cpKv * (vdd - 3.3));
    if( mode ==  mlx90640.calibrationModeEE)
    {
        irDataCP[1] = irDataCP[1] - mlx90640.cpOffset[1] * (1 + mlx90640.cpKta * (ta - 25)) * (1 + mlx90640.cpKv * (vdd - 3.3));
    }
    else
    {
      irDataCP[1] = irDataCP[1] - (mlx90640.cpOffset[1] + mlx90640.ilChessC[0]) * (1 + mlx90640.cpKta * (ta - 25)) * (1 + mlx90640.cpKv * (vdd - 3.3));
    }

    for( int pixelNumber = 0; pixelNumber < 768; pixelNumber++)
    {
        ilPattern = pixelNumber / 32 - (pixelNumber / 64) * 2;
        chessPattern = ilPattern ^ (pixelNumber - (pixelNumber/2)*2);
        conversionPattern = ((pixelNumber + 2) / 4 - (pixelNumber + 3) / 4 + (pixelNumber + 1) / 4 - pixelNumber / 4) * (1 - 2 * ilPattern);

        if(mode == 0)
        {
          pattern = ilPattern;
        }
        else
        {
          pattern = chessPattern;
        }

        if(pattern == frameData[833])
        {
            irData = frameData[pixelNumber];
            if(irData > 32767)
            {
                irData = irData - 65536;
            }
            irData = irData * gain;

            kta = mlx90640.kta[pixelNumber]/ktaScale;
            kv = mlx90640.kv[pixelNumber]/kvScale;
            irData = irData - mlx90640.offset[pixelNumber]*(1 + kta*(ta - 25))*(1 + kv*(vdd - 3.3));

            if(mode !=  mlx90640.calibrationModeEE)
            {
              irData = irData + mlx90640.ilChessC[2] * (2 * ilPattern - 1) - mlx90640.ilChessC[1] * conversionPattern;
            }

            irData = irData - mlx90640.tgc * irDataCP[subPage];
            irData = irData / emissivity;

            alphaCompensated = SCALEALPHA*alphaScale/mlx90640.alpha[pixelNumber];
            alphaCompensated = alphaCompensated*(1 + mlx90640.KsTa * (ta - 25));

            Sx = alphaCompensated * alphaCompensated * alphaCompensated * (irData + alphaCompensated * taTr);
            Sx = sqrt(sqrt(Sx)) * mlx90640.ksTo[1];

            To = sqrt(sqrt(irData/(alphaCompensated * (1 - mlx90640.ksTo[1] * 273.15) + Sx) + taTr)) - 273.15;

            if(To < mlx90640.ct[1])
            {
                range = 0;
            }
            else if(To < mlx90640.ct[2])
            {
                range = 1;
            }
            else if(To < mlx90640.ct[3])
            {
                range = 2;
            }
            else
            {
                range = 3;
            }

            To = sqrt(sqrt(irData / (alphaCompensated * alphaCorrR[range] * (1 + mlx90640.ksTo[range] * (To - mlx90640.ct[range]))) + taTr)) - 273.15;

            result[pixelNumber] = To;
        }
    }
}

/*!
 *    @brief  Read 2 pages, calculate temperatures and place into framebuf
 *    @param  framebuf 24*32 floating point memory buffer
 *    @return 0 on success
 */
int MLX90640_getFrame() {
  float emissivity = 0.95;
  float tr = 23.15;
  float ta = 0.0;
  int status = 0;
  uint8_t pagesGot = 0;

  int numTried = 0;
  while (pagesGot != 0x03 && numTried < 20) {  // loop until both subpages captured
	  status = MLX90640_GetFrameData();
	  if (status < 0) return status;
	  numTried++;

	  uint8_t subpage = frameData[833] & 0x01;
	  if (pagesGot & (1 << subpage)) continue;  // already have this one, retry

	  pagesGot |= (1 << subpage);
	  ta = MLX90640_GetTa();
	  tr = ta - OPENAIR_TA_SHIFT;
	  MLX90640_CalculateTo(emissivity, tr, tempBuf);
  }

//  for (uint8_t page = 0; page < 2; page++) {
//    status = MLX90640_GetFrameData();
//    if (status < 0) return status;
//
//    ta = MLX90640_GetTa();
//    tr = ta - OPENAIR_TA_SHIFT;
//    MLX90640_CalculateTo(emissivity, tr, tempBuf);
//  }

  /* Convert float32 -> float16 and store */
  for (int i = 0; i < 24 * 32; i++) {
    frameBuf[i] = f32_to_f16(tempBuf[i]);
  }
  return 0;
}

int MLX90640_getRawFrame()
{
	int status;

  for (uint8_t page = 0; page < 2; page++) {
	status = MLX90640_GetFrameData();

	if (status < 0) {
	  return status;
	}
  }

  return 0;
}

void MLX90640_diagnostic_test(void)
{
    char buf[100];
    uint16_t testValue;
    HAL_StatusTypeDef status;

    HAL_UART_Transmit(&huart2, StartMSG, sizeof(StartMSG), 10000);
    uint8_t i = 0, ret;
    for(i=1; i<128; i++)
	{
		ret = HAL_I2C_IsDeviceReady(&hi2c1, (uint16_t)(i<<1), 3, 5);
		if (ret != HAL_OK) /* No ACK Received At That Address */
		{
			HAL_UART_Transmit(&huart2, Space, sizeof(Space), 10000);
		}
		else if(ret == HAL_OK)
		{
			sprintf(Buffer, "0x%X", i);
			HAL_UART_Transmit(&huart2, Buffer, sizeof(Buffer), 10000);
		}
	}
	HAL_UART_Transmit(&huart2, EndMSG, sizeof(EndMSG), 10000);
	HAL_Delay(100);

	// try to read serial number from sensor
	uint16_t serialNo;
	MLX_ReadReg(&hi2c1, 0x2407, &serialNo);

	HAL_Delay(100);
	char serialNoBuf[6];
	itoa(serialNo, serialNoBuf, 16);
	serialNoBuf[5] = "\n";
	HAL_UART_Transmit(&huart2, serialNoBuf, sizeof(serialNoBuf), 10000);

    // Test 1: Read a known EEPROM register
    status = MLX_ReadReg(&hi2c1, 0x2400, &testValue);
    sprintf(buf, "EEPROM[0x2400]: 0x%04X, Status: %d\n", testValue, status);
    HAL_UART_Transmit(&huart2, (uint8_t*)buf, strlen(buf), 10000);

    // Test 2: Read status register
    status = MLX_ReadReg(&hi2c1, 0x8000, &testValue);
    sprintf(buf, "Status[0x8000]: 0x%04X, Status: %d\n", testValue, status);
    HAL_UART_Transmit(&huart2, (uint8_t*)buf, strlen(buf), 10000);

    // Test 3: Read first frame pixel
    status = MLX_ReadReg(&hi2c1, 0x0400, &testValue);
    sprintf(buf, "Frame[0x0400]: 0x%04X, Status: %d\n", testValue, status);
    HAL_UART_Transmit(&huart2, (uint8_t*)buf, strlen(buf), 10000);

    // Test 4: Try reading 10 words with block read
    uint16_t testBlock[10];
    status = MLX_ReadBlock(&hi2c1, 0x2400, 10, testBlock);
    sprintf(buf, "Block read status: %d\n", status);
    HAL_UART_Transmit(&huart2, (uint8_t*)buf, strlen(buf), 10000);

    for (int i = 0; i < 10; i++) {
        sprintf(buf, "  [%d]: 0x%04X\n", i, testBlock[i]);
        HAL_UART_Transmit(&huart2, (uint8_t*)buf, strlen(buf), 10000);
    }
}

/**
 * Will print the contents of the EEPROM to UART. Use for debugging.
 */
void MLX90640_Dump_EEPROM()
{
	char regBuf[32] = { 0 };
	for (int i = 0; i < 832; i++) {
		sprintf(regBuf, "0x%04X\n", eepromData[i]);
		HAL_UART_Transmit(&huart2, regBuf, sizeof(regBuf), 10000);
	}
}

/**
 * Performs sensor initialization. Reads required information from EEPROM, and then
 * sets various settings (chess mode for readout pattern, 18-bit resolution, 4Hz refresh rate
 */
void MLX90640_InitSensor(uint8_t debug)
{
	MLX_ReadBlock(&hi2c1, 0x2400, 832, eepromData);
  	// set to chess mode
	if (debug)
		HAL_UART_Transmit(&huart2, ModeSetMsg, sizeof(ModeSetMsg), 10000);
  	uint16_t controlRegister1;

  	HAL_StatusTypeDef status = MLX_ReadReg(&hi2c1, 0x800D, &controlRegister1);
  	if (status != HAL_OK) {
  		HAL_UART_Transmit(&huart2, ErrorMsgRead, sizeof(ErrorMsgRead), 10000);
  		char buf[40];
  		                       	    sprintf(buf, "I2C err: %d, ISR: 0x%08lX\n", status, hi2c1.Instance->ISR);
  		                       	    HAL_UART_Transmit(&huart2, buf, strlen(buf), 10000);
  	}

	uint16_t value = (controlRegister1 | 0x1000);
	status = MLX_WriteReg(&hi2c1, 0x800D, value);
	if (status != HAL_OK) {
		HAL_UART_Transmit(&huart2, ErrorMsgWrite, sizeof(ErrorMsgWrite), 10000);
	}

	// verify mode set
	status = MLX_ReadReg(&hi2c1, 0x800D, &controlRegister1);
	int modeRAM = (controlRegister1 & 0x1000) >> 12;
	if (modeRAM == 1 && debug) {
		HAL_UART_Transmit(&huart2, ChessMsg, sizeof(ChessMsg), 10000);
	}
	if (debug)
		HAL_UART_Transmit(&huart2, ModeSetMsg, sizeof(ModeSetMsg), 10000);

  	// set resolution
	if (debug)
		HAL_UART_Transmit(&huart2, ResolutionSetMsg, sizeof(ResolutionSetMsg), 10000);
  	status = MLX_ReadReg(&hi2c1, 0x800D, &controlRegister1);
  	if (status != HAL_OK) {
  		HAL_UART_Transmit(&huart2, ErrorMsgRead, sizeof(ErrorMsgRead), 10000);
  	}

  	uint8_t resolution = 2; // 18-bit
  	value = (resolution & 0x03) << 10;
  	value = (controlRegister1 & 0xF3FF) | value;
  	status = MLX_WriteReg(&hi2c1, 0x800D, value);
  	if (status != HAL_OK) {
		HAL_UART_Transmit(&huart2, ErrorMsgWrite, sizeof(ErrorMsgWrite), 10000);
	}
  	if (debug)
  		HAL_UART_Transmit(&huart2, ResolutionSetMsg, sizeof(ResolutionSetMsg), 10000);

  	// set frame rate
  	if (debug)
  		HAL_UART_Transmit(&huart2, FrameRateSetMsg, sizeof(FrameRateSetMsg), 10000);
  	uint8_t refreshRate = 2; // 4Hz
  	value = (refreshRate & 0x07)<<7;

	status = MLX_ReadReg(&hi2c1, 0x800D, &controlRegister1);
	if (status != HAL_OK) {
		HAL_UART_Transmit(&huart2, ErrorMsgRead, sizeof(ErrorMsgRead), 10000);
	}
	value = (controlRegister1 & 0xFC7F) | value;
	status = MLX_WriteReg(&hi2c1, 0x800D, value);
	if (status != HAL_OK) {
		HAL_UART_Transmit(&huart2, ErrorMsgWrite, sizeof(ErrorMsgWrite), 10000);
	}

	status = MLX_ReadReg(&hi2c1, 0x800D, &controlRegister1);
	if (status != HAL_OK) {
		HAL_UART_Transmit(&huart2, ErrorMsgRead, sizeof(ErrorMsgRead), 10000);
	}
	// verify rate
	int rate = (controlRegister1 & 0x0380) >> 7;
	if (debug) {
		if (rate == 2) {
			uint8_t tempbuftwo[] = "RATE 2 Hz\n";
			HAL_UART_Transmit(&huart2, tempbuftwo, sizeof(tempbuftwo), 10000);
		} else if (rate == 4) {
			uint8_t tempbuftwo[] = "RATE 4 Hz\n";
			HAL_UART_Transmit(&huart2, tempbuftwo, sizeof(tempbuftwo), 10000);
		}
		HAL_UART_Transmit(&huart2, FrameRateSetMsg, sizeof(FrameRateSetMsg), 10000);
	}
}

void I2C_BusRecovery(void) {
    // Configure SCL and SDA as GPIO outputs temporarily
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_7;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    // Clock 9 times to release stuck slave
    for (int i = 0; i < 9; i++) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
        HAL_Delay(1);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_RESET);
        HAL_Delay(1);
    }
    // Send STOP condition
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_RESET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_6, GPIO_PIN_SET);
    HAL_Delay(1);
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_7, GPIO_PIN_SET);
    HAL_Delay(1);

    // Re-initialize I2C peripheral
    HAL_I2C_DeInit(&hi2c1);
    MX_I2C1_Init();
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

	struct msgPayload payload = { 0 };
	payload.moduleID = 0x14;
	payload.row = 1;

	payload.hr = 3;
	payload.min = 6;
	payload.sec = 30;

	payload.volt = 3.5;

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
  MX_DMA_Init();
  MX_USART2_UART_Init();
  MX_TIM1_Init();
  MX_UART4_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  MX_TIM3_Init();
  MX_I2C1_Init();
  MX_UART5_Init();
  /* USER CODE BEGIN 2 */

  HAL_TIM_PWM_Start(SERVO_TIMER, SERVO_CHANNEL); //Start TIM1 for servo timer
//  HAL_TIM_PWM_Start(FAN_TIMER, FAN1_CHANNEL); //Start TIM2 for fan1 timer
//  HAL_TIM_PWM_Start(FAN_TIMER, FAN2_CHANNEL); //Start TIM2 for fan2 timer

  HAL_ADCEx_Calibration_Start(GAS_SENSOR_ADC, ADC_SINGLE_ENDED); //Calibrate ADC

  DHT22_Init(); //Initialize DHT22 Sensor

  uint8_t gpsBuff[255];
  HAL_UART_Receive_DMA(GPS_UART, gpsBuff, 255);

  // scans for MLX90640 i2c address and runs some basic tests

  MLX90640_InitSensor(1);

  // extract calibration parameters from EEPROM
  MLX90640_ExtractParameters(eepromData);

//  MLX90640_Dump_EEPROM();

  MLX90640_diagnostic_test();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {

	  MLX90640_getFrame();

	  for (int i = 0; i < 8; i++) {
	  // Turn Fans On
//		  __HAL_TIM_SET_COMPARE(FAN_TIMER, FAN1_CHANNEL, FAN_ON);
//		  __HAL_TIM_SET_COMPARE(FAN_TIMER, FAN2_CHANNEL, FAN_ON);

		  HAL_Delay(100);  // arbitrary delay value


	  //Take ADC readings for Gas Sensor
		  HAL_ADC_Start(GAS_SENSOR_ADC);
		  if (HAL_ADC_PollForConversion(GAS_SENSOR_ADC, 100) == HAL_OK)
		  {
			  // Get gas sensor reading
			  adc_value = HAL_ADC_GetValue(GAS_SENSOR_ADC);
			  payload.gas = adc_value;

			  // Print reading to console
			  int len = snprintf(buff, sizeof(buff), "GAS ADC Value: %d\r\n", adc_value);
			  HAL_UART_Transmit(COM_UART, (uint8_t *)buff, len, HAL_MAX_DELAY);
		  }
		  HAL_ADC_Stop(GAS_SENSOR_ADC);
		  HAL_Delay(100);

	  //Take Readings from temperature/humidity sensor
		  DHT22_Data_t dht = DHT22_GetData();
		  if (dht.valid)
		  {
			  int len = snprintf(buff, sizeof(buff), "Temp: %.1fC  Hum: %.1f%%\r\n", dht.temperature_C, dht.humidity);
			  payload.temp = dht.temperature_C;
			  payload.hum = dht.humidity;


		      HAL_UART_Transmit(COM_UART, (uint8_t *)buff, len, HAL_MAX_DELAY);
		  }
		  HAL_Delay(100);  // arbitrary delay value

	  //Take GPS Time Readings

		  if (flag == 1)
		  {
			  char *g = strstr((char*)gpsBuff, "$GPGGA");

			  // prints raw data to debug gps
			  HAL_UART_Transmit(COM_UART, (uint8_t*)gpsBuff, strlen((char*)gpsBuff), 200);
			  HAL_UART_Transmit(COM_UART, (uint8_t*)"\r\n", 2, 200);



			  if (g != NULL)
			  {
				  char *utc = strchr(g, ',');
				  if (utc != NULL && strlen(utc) >= 7)
				  {
					  utc++;

					  char hh[3] = {utc[0], utc[1], '\0'};
					  char mm[3] = {utc[2], utc[3], '\0'};
					  char ss[3] = {utc[4], utc[5], '\0'};

					  // variables with integer values for time
//					  payload.hr = 00;
//					  payload.min = 00;
//					  payload.sec = 00;

					  // prints time to console
					  char strUTC[9];
					  snprintf(strUTC, sizeof(strUTC), "%s:%s:%s", hh, mm, ss);
					  HAL_UART_Transmit(COM_UART, (uint8_t*)"UTC: ", 5, 200);
					  HAL_UART_Transmit(COM_UART, (uint8_t*)strUTC, 8, 200);
					  HAL_UART_Transmit(COM_UART, (uint8_t*)"\r\n", 2, 200);

				  }
			  }

			  flag = 0;
			  HAL_UART_Receive_DMA(GPS_UART, gpsBuff, 255);
		  }


		  payload.row = i;
		  memcpy(payload.pixels, (frameBuf + i*32), 32*2*3);

		  HAL_StatusTypeDef uartstatus = HAL_UART_Transmit(&huart5, (uint8_t *)&payload, sizeof(payload), 10000);
		  if (uartstatus != HAL_OK) {
			  char loraerrorbuf[] = "Error transmitting to lora module!\n";
			  HAL_UART_Transmit(&huart2, loraerrorbuf, sizeof(loraerrorbuf), 10000);
		  }
		  HAL_Delay(200);
	  }

	  float temp;
	  int j, k;
	  char rowBuf[34] = { 0 };
	  	for (j = 0; j < 24; j++) {
	  		rowBuf[0] = '\n';
	  		for (k = 0; k < 32; k++) {
	  			temp = f16_to_f32(frameBuf[j * 32 + k]);
	  			if (temp < 10) rowBuf[k] = '=';
	  			else if (temp < 20) rowBuf[k] = '0';
	  			  else if (temp < 23) rowBuf[k] = '.';
	  			  else if (temp < 25) rowBuf[k] = '-';
	  			  else if (temp < 27) rowBuf[k] = '*';
	  			  else if (temp < 29) rowBuf[k] = '+';
	  			  else if (temp < 31) rowBuf[k] = 'x';
	  			  else if (temp < 33) rowBuf[k] = '%';
	  			  else if (temp < 35) rowBuf[k] = '#';
	  			  else if (temp < 37) rowBuf[k] = 'X';
	  			  else rowBuf[k] = 'N';
	  		}
	  		rowBuf[32] = '\n';
	  		HAL_UART_Transmit(&huart2, rowBuf, 33, 100000);

	  	}

	  	 HAL_Delay(5000);



	  // Move servo position continuously between 0, 90, 180 degrees
	  	// 0 degrees
//		__HAL_TIM_SET_COMPARE(SERVO_TIMER, SERVO_CHANNEL, SERVO_0_DEG);
//		HAL_Delay(SERVO_DELAY_MS);

		// 90 degrees
//		__HAL_TIM_SET_COMPARE(SERVO_TIMER, SERVO_CHANNEL, SERVO_90_DEG);
//		HAL_Delay(SERVO_DELAY_MS);

		// 180 degrees
//		__HAL_TIM_SET_COMPARE(SERVO_TIMER, SERVO_CHANNEL, SERVO_180_DEG);
//		HAL_Delay(SERVO_DELAY_MS);

		// Back to 0 degrees
//		__HAL_TIM_SET_COMPARE(SERVO_TIMER, SERVO_CHANNEL, SERVO_0_DEG);
//		HAL_Delay(SERVO_DELAY_MS);

	// Turn Fans Off
//		__HAL_TIM_SET_COMPARE(FAN_TIMER, FAN1_CHANNEL, FAN_OFF);
//		__HAL_TIM_SET_COMPARE(FAN_TIMER, FAN2_CHANNEL, FAN_OFF);

//		HAL_Delay(2000);


	// Send Data

    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
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

  /** Configure the main internal regulator output voltage
  */
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 10;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV7;
  RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
  RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
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

  ADC_MultiModeTypeDef multimode = {0};
  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ClockPrescaler = ADC_CLOCK_ASYNC_DIV1;
  hadc1.Init.Resolution = ADC_RESOLUTION_12B;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
  hadc1.Init.LowPowerAutoWait = DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.NbrOfConversion = 1;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_NONE;
  hadc1.Init.DMAContinuousRequests = DISABLE;
  hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
  hadc1.Init.OversamplingMode = DISABLE;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure the ADC multi-mode
  */
  multimode.Mode = ADC_MODE_INDEPENDENT;
  if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_4;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_247CYCLES_5;
  sConfig.SingleDiff = ADC_SINGLE_ENDED;
  sConfig.OffsetNumber = ADC_OFFSET_NONE;
  sConfig.Offset = 0;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

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
  hi2c1.Init.Timing = 0x10D19CE4;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 79;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 19999;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.BreakFilter = 0;
  sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
  sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
  sBreakDeadTimeConfig.Break2Filter = 0;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_ENABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

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
  TIM_OC_InitTypeDef sConfigOC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 39;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 100;
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
  if (HAL_TIM_PWM_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */
  HAL_TIM_MspPostInit(&htim2);

}

/**
  * @brief TIM3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM3_Init(void)
{

  /* USER CODE BEGIN TIM3_Init 0 */

  /* USER CODE END TIM3_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM3_Init 1 */

  /* USER CODE END TIM3_Init 1 */
  htim3.Instance = TIM3;
  htim3.Init.Prescaler = 79;
  htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim3.Init.Period = 65535;
  htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim3) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim3, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM3_Init 2 */

  /* USER CODE END TIM3_Init 2 */

}

/**
  * @brief UART4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART4_Init(void)
{

  /* USER CODE BEGIN UART4_Init 0 */

  /* USER CODE END UART4_Init 0 */

  /* USER CODE BEGIN UART4_Init 1 */

  /* USER CODE END UART4_Init 1 */
  huart4.Instance = UART4;
  huart4.Init.BaudRate = 9600;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  huart4.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart4.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART4_Init 2 */

  /* USER CODE END UART4_Init 2 */

}

/**
  * @brief UART5 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART5_Init(void)
{

  /* USER CODE BEGIN UART5_Init 0 */

  /* USER CODE END UART5_Init 0 */

  /* USER CODE BEGIN UART5_Init 1 */

  /* USER CODE END UART5_Init 1 */
  huart5.Instance = UART5;
  huart5.Init.BaudRate = 115200;
  huart5.Init.WordLength = UART_WORDLENGTH_8B;
  huart5.Init.StopBits = UART_STOPBITS_1;
  huart5.Init.Parity = UART_PARITY_NONE;
  huart5.Init.Mode = UART_MODE_TX_RX;
  huart5.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart5.Init.OverSampling = UART_OVERSAMPLING_16;
  huart5.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart5.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart5) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART5_Init 2 */

  /* USER CODE END UART5_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

}

/**
  * Enable DMA controller clock
  */
static void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Channel5_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Channel5_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(DMA2_Channel5_IRQn);

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
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, LD2_Pin|GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD2_Pin PA15 */
  GPIO_InitStruct.Pin = LD2_Pin|GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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

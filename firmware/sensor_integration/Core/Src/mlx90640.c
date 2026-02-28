/**
 ******************************************************************************
 * @file    mlx90640.c
 * @brief   MLX90640 thermal camera driver implementation
 ******************************************************************************
 */

#include "mlx90640.h"
#include <stdio.h>
#include <stdlib.h>

/* ── Private: active I2C handle ───────────────────────────────────────────── */
static I2C_HandleTypeDef *thermal_i2c = NULL;

/* ── Public data ──────────────────────────────────────────────────────────── */
paramsMLX90640 mlx90640    = {0};
float          tempBuf[24 * 32]  = {0};
uint16_t       frameBuf[24 * 32] = {0};
uint16_t       frameData[834]    = {0};
uint16_t       eepromData[832]   = {0};
float          scratchData[768];          // internal scratch buffer

/* ── Debug UART handle (set by application if needed) ─────────────────────── */
extern UART_HandleTypeDef huart2;         // keep using the same UART as before

/* ── UART message strings ─────────────────────────────────────────────────── */
static uint8_t StartMSG[]        = "Starting I2C Scanning: \r\n";
static uint8_t EndMSG[]          = "Done! \r\n\r\n";
static uint8_t Space[]           = " - ";
static uint8_t Buffer[25]        = {0};
static uint8_t StartFrameMsg[]   = "\nStarting to read frame: \n";
static uint8_t EndFrameMsg[]     = "\nFinished frame read!\n";
static uint8_t ModeSetMsg[]      = "\nSet chess mode\n";
static uint8_t ResolutionSetMsg[]= "\nSet resolution\n";
static uint8_t FrameRateSetMsg[] = "\nSet frame rate\n";
static uint8_t ErrorMsgRead[]    = "\nError in read\n";
static uint8_t ErrorMsgWrite[]   = "\nError in write\n";
static uint8_t ChessMsg[]        = "\nCHESS\n";

/* ============================================================================
 *  Initialisation
 * ========================================================================== */

void MLX90640_SetI2C(I2C_HandleTypeDef *hi2c)
{
    thermal_i2c = hi2c;
}

/* ============================================================================
 *  float16 helpers
 * ========================================================================== */

uint16_t f32_to_f16(float f)
{
    uint32_t x;
    memcpy(&x, &f, sizeof(x));

    uint16_t sign = (x >> 16) & 0x8000;
    int32_t  exp  = ((x >> 23) & 0xFF) - 127 + 15;
    uint32_t mant = x & 0x007FFFFF;

    if (exp <= 0)  return sign;
    if (exp >= 31) return sign | 0x7C00;
    return sign | ((uint16_t)exp << 10) | (uint16_t)(mant >> 13);
}

float f16_to_f32(uint16_t h)
{
    uint32_t sign = (uint32_t)(h & 0x8000) << 16;
    uint32_t exp  = (h >> 10) & 0x1F;
    uint32_t mant = h & 0x03FF;
    uint32_t x;

    if      (exp == 0)  x = sign;
    else if (exp == 31) x = sign | 0x7F800000 | (mant << 13);
    else                x = sign | ((exp - 15 + 127) << 23) | (mant << 13);

    float f;
    memcpy(&f, &x, sizeof(f));
    return f;
}

/* ============================================================================
 *  Low-level I2C helpers
 * ========================================================================== */

HAL_StatusTypeDef MLX_ReadReg(uint16_t reg, uint16_t *value)
{
    uint8_t buf[2];
    HAL_StatusTypeDef st = HAL_I2C_Mem_Read(thermal_i2c, MLX90640_I2C_ADDR,
                                             reg, I2C_MEMADD_SIZE_16BIT,
                                             buf, 2, 1000);
    if (st != HAL_OK) return st;
    *value = (buf[0] << 8) | buf[1];
    return HAL_OK;
}

HAL_StatusTypeDef MLX_WriteReg(uint16_t reg, uint16_t value)
{
    uint8_t buf[2] = { value >> 8, value & 0xFF };
    return HAL_I2C_Mem_Write(thermal_i2c, MLX90640_I2C_ADDR,
                             reg, I2C_MEMADD_SIZE_16BIT,
                             buf, 2, 1000);
}

HAL_StatusTypeDef MLX_ReadBlock(uint16_t startReg, uint16_t words, uint16_t *dest)
{
    uint8_t  raw[64];
    uint16_t remaining = words;
    uint16_t offset    = 0;

    while (remaining)
    {
        uint16_t chunkWords = (remaining > 32) ? 32 : remaining;
        uint16_t chunkBytes = chunkWords * 2;

        HAL_StatusTypeDef st = HAL_I2C_Mem_Read(thermal_i2c, MLX90640_I2C_ADDR,
                                                 startReg + offset,
                                                 I2C_MEMADD_SIZE_16BIT,
                                                 raw, chunkBytes, 2000);
        if (st != HAL_OK) return st;

        for (uint16_t i = 0; i < chunkWords; i++)
            dest[offset + i] = (raw[2 * i] << 8) | raw[2 * i + 1];

        offset    += chunkWords;
        remaining -= chunkWords;
    }
    return HAL_OK;
}

/* ============================================================================
 *  EEPROM parameter extraction
 * ========================================================================== */

static void ExtractVDDParameters(uint16_t *eeData)
{
    int16_t kVdd = (eeData[51] & 0xFF00) >> 8;
    if (kVdd > 127) kVdd -= 256;
    kVdd *= 32;

    int16_t vdd25 = eeData[51] & 0x00FF;
    vdd25 = ((vdd25 - 256) << 5) - 8192;

    mlx90640.kVdd  = kVdd;
    mlx90640.vdd25 = vdd25;
}

static void ExtractPTATParameters(uint16_t *eeData)
{
    float KvPTAT = (eeData[50] & 0xFC00) >> 10;
    if (KvPTAT > 31) KvPTAT -= 64;
    KvPTAT /= 4096.0f;

    float KtPTAT = eeData[50] & 0x03FF;
    if (KtPTAT > 511) KtPTAT -= 1024;
    KtPTAT /= 8.0f;

    int16_t vPTAT25  = eeData[49];
    float alphaPTAT  = (eeData[16] & 0xF000) / pow(2.0, 14) + 8.0f;

    mlx90640.KvPTAT    = KvPTAT;
    mlx90640.KtPTAT    = KtPTAT;
    mlx90640.vPTAT25   = vPTAT25;
    mlx90640.alphaPTAT = alphaPTAT;
}

static void ExtractGainParameters(uint16_t *eeData)
{
    int16_t gainEE = eeData[48];
    if (gainEE > 32767) gainEE -= 65536;
    mlx90640.gainEE = gainEE;
}

static void ExtractTgcParameters(uint16_t *eeData)
{
    float tgc = eeData[60] & 0x00FF;
    if (tgc > 127) tgc -= 256;
    mlx90640.tgc = tgc / 32.0f;
}

static void ExtractResolutionParameters(uint16_t *eeData)
{
    mlx90640.resolutionEE = (eeData[56] & 0x3000) >> 12;
}

static void ExtractKsTaParameters(uint16_t *eeData)
{
    float KsTa = (eeData[60] & 0xFF00) >> 8;
    if (KsTa > 127) KsTa -= 256;
    mlx90640.KsTa = KsTa / 8192.0f;
}

static void ExtractKsToParameters(uint16_t *eeData)
{
    int8_t step = ((eeData[63] & 0x3000) >> 12) * 10;

    mlx90640.ct[0] = -40;
    mlx90640.ct[1] = 0;
    mlx90640.ct[2] = ((eeData[63] & 0x00F0) >> 4) * step;
    mlx90640.ct[3] = mlx90640.ct[2] + ((eeData[63] & 0x0F00) >> 8) * step;
    mlx90640.ct[4] = 400;

    int KsToScale = 1 << ((eeData[63] & 0x000F) + 8);

    mlx90640.ksTo[0] = eeData[61] & 0x00FF;
    mlx90640.ksTo[1] = (eeData[61] & 0xFF00) >> 8;
    mlx90640.ksTo[2] = eeData[62] & 0x00FF;
    mlx90640.ksTo[3] = (eeData[62] & 0xFF00) >> 8;

    for (int i = 0; i < 4; i++)
    {
        if (mlx90640.ksTo[i] > 127) mlx90640.ksTo[i] -= 256;
        mlx90640.ksTo[i] /= KsToScale;
    }
    mlx90640.ksTo[4] = -0.0002f;
}

static void ExtractCPParameters(uint16_t *eeData)
{
    float   alphaSP[2];
    int16_t offsetSP[2];
    uint8_t alphaScale = ((eeData[32] & 0xF000) >> 12) + 27;

    offsetSP[0] = eeData[58] & 0x03FF;
    if (offsetSP[0] > 511) offsetSP[0] -= 1024;

    offsetSP[1] = (eeData[58] & 0xFC00) >> 10;
    if (offsetSP[1] > 31) offsetSP[1] -= 64;
    offsetSP[1] += offsetSP[0];

    alphaSP[0] = eeData[57] & 0x03FF;
    if (alphaSP[0] > 511) alphaSP[0] -= 1024;
    alphaSP[0] /= pow(2.0, alphaScale);

    alphaSP[1] = (eeData[57] & 0xFC00) >> 10;
    if (alphaSP[1] > 31) alphaSP[1] -= 64;
    alphaSP[1] = (1 + alphaSP[1] / 128.0f) * alphaSP[0];

    float   cpKta     = eeData[59] & 0x00FF;
    if (cpKta > 127) cpKta -= 256;
    uint8_t ktaScale1 = ((eeData[56] & 0x00F0) >> 4) + 8;
    mlx90640.cpKta = cpKta / pow(2.0, ktaScale1);

    float   cpKv    = (eeData[59] & 0xFF00) >> 8;
    if (cpKv > 127) cpKv -= 256;
    uint8_t kvScale = (eeData[56] & 0x0F00) >> 8;
    mlx90640.cpKv = cpKv / pow(2.0, kvScale);

    mlx90640.cpAlpha[0]  = alphaSP[0];
    mlx90640.cpAlpha[1]  = alphaSP[1];
    mlx90640.cpOffset[0] = offsetSP[0];
    mlx90640.cpOffset[1] = offsetSP[1];
}

static void ExtractAlphaParameters(uint16_t *eeData)
{
    int16_t accRow[24], accColumn[32];
    int     p = 0;
    uint8_t accRemScale    = eeData[32] & 0x000F;
    uint8_t accColumnScale = (eeData[32] & 0x00F0) >> 4;
    uint8_t accRowScale    = (eeData[32] & 0x0F00) >> 8;
    uint8_t alphaScale     = ((eeData[32] & 0xF000) >> 12) + 30;
    int     alphaRef       = eeData[33];

    for (int i = 0; i < 6; i++)
    {
        p = i * 4;
        accRow[p+0] =  eeData[34+i] & 0x000F;
        accRow[p+1] = (eeData[34+i] & 0x00F0) >> 4;
        accRow[p+2] = (eeData[34+i] & 0x0F00) >> 8;
        accRow[p+3] = (eeData[34+i] & 0xF000) >> 12;
    }
    for (int i = 0; i < 24; i++) if (accRow[i] > 7) accRow[i] -= 16;

    for (int i = 0; i < 8; i++)
    {
        p = i * 4;
        accColumn[p+0] =  eeData[40+i] & 0x000F;
        accColumn[p+1] = (eeData[40+i] & 0x00F0) >> 4;
        accColumn[p+2] = (eeData[40+i] & 0x0F00) >> 8;
        accColumn[p+3] = (eeData[40+i] & 0xF000) >> 12;
    }
    for (int i = 0; i < 32; i++) if (accColumn[i] > 7) accColumn[i] -= 16;

    for (int i = 0; i < 24; i++)
        for (int j = 0; j < 32; j++)
        {
            p = 32 * i + j;
            scratchData[p] = (eeData[64+p] & 0x03F0) >> 4;
            if (scratchData[p] > 31) scratchData[p] -= 64;
            scratchData[p] *= (1 << accRemScale);
            scratchData[p]  = alphaRef + (accRow[i] << accRowScale)
                            + (accColumn[j] << accColumnScale) + scratchData[p];
            scratchData[p] /= pow(2.0, alphaScale);
            scratchData[p] -= mlx90640.tgc * (mlx90640.cpAlpha[0] + mlx90640.cpAlpha[1]) / 2.0f;
            scratchData[p]  = SCALEALPHA / scratchData[p];
        }

    float temp = scratchData[0];
    for (int i = 1; i < 768; i++)
        if (scratchData[i] > temp) temp = scratchData[i];

    alphaScale = 0;
    if (temp <= 0) temp = 1;
    while (temp < 32768) { temp *= 2; alphaScale++; }

    for (int i = 0; i < 768; i++)
        mlx90640.alpha[i] = (uint16_t)(scratchData[i] * pow(2.0, alphaScale) + 0.5f);

    mlx90640.alphaScale = alphaScale;
}

static void ExtractOffsetParameters(uint16_t *eeData)
{
    int16_t occRow[24], occColumn[32];
    int     p = 0;

    uint8_t occRemScale    = eeData[16] & 0x000F;
    uint8_t occColumnScale = (eeData[16] & 0x00F0) >> 4;
    uint8_t occRowScale    = (eeData[16] & 0x0F00) >> 8;
    int16_t offsetRef      = eeData[17];
    if (offsetRef > 32767) offsetRef -= 65536;

    for (int i = 0; i < 6; i++)
    {
        p = i * 4;
        occRow[p+0] =  eeData[18+i] & 0x000F;
        occRow[p+1] = (eeData[18+i] & 0x00F0) >> 4;
        occRow[p+2] = (eeData[18+i] & 0x0F00) >> 8;
        occRow[p+3] = (eeData[18+i] & 0xF000) >> 12;
    }
    for (int i = 0; i < 24; i++) if (occRow[i] > 7) occRow[i] -= 16;

    for (int i = 0; i < 8; i++)
    {
        p = i * 4;
        occColumn[p+0] =  eeData[24+i] & 0x000F;
        occColumn[p+1] = (eeData[24+i] & 0x00F0) >> 4;
        occColumn[p+2] = (eeData[24+i] & 0x0F00) >> 8;
        occColumn[p+3] = (eeData[24+i] & 0xF000) >> 12;
    }
    for (int i = 0; i < 32; i++) if (occColumn[i] > 7) occColumn[i] -= 16;

    for (int i = 0; i < 24; i++)
        for (int j = 0; j < 32; j++)
        {
            p = 32 * i + j;
            mlx90640.offset[p] = (eeData[64+p] & 0xFC00) >> 10;
            if (mlx90640.offset[p] > 31) mlx90640.offset[p] -= 64;
            mlx90640.offset[p] *= (1 << occRemScale);
            mlx90640.offset[p]  = offsetRef + (occRow[i] << occRowScale)
                                 + (occColumn[j] << occColumnScale) + mlx90640.offset[p];
        }
}

static void ExtractKtaPixelParameters(uint16_t *eeData)
{
    int8_t  KtaRC[4];
    uint8_t ktaScale1 = ((eeData[56] & 0x00F0) >> 4) + 8;
    uint8_t ktaScale2 = eeData[56] & 0x000F;

    int8_t KtaRoCo = (eeData[54] & 0xFF00) >> 8; if (KtaRoCo > 127) KtaRoCo -= 256; KtaRC[0] = KtaRoCo;
    int8_t KtaReCo = (eeData[54] & 0x00FF);       if (KtaReCo > 127) KtaReCo -= 256; KtaRC[2] = KtaReCo;
    int8_t KtaRoCe = (eeData[55] & 0xFF00) >> 8; if (KtaRoCe > 127) KtaRoCe -= 256; KtaRC[1] = KtaRoCe;
    int8_t KtaReCe = (eeData[55] & 0x00FF);       if (KtaReCe > 127) KtaReCe -= 256; KtaRC[3] = KtaReCe;

    for (int i = 0; i < 24; i++)
        for (int j = 0; j < 32; j++)
        {
            int     p     = 32 * i + j;
            uint8_t split = 2 * (p / 32 - (p / 64) * 2) + p % 2;
            scratchData[p] = (eeData[64+p] & 0x000E) >> 1;
            if (scratchData[p] > 3) scratchData[p] -= 8;
            scratchData[p] = (KtaRC[split] + scratchData[p] * (1 << ktaScale2))
                             / pow(2.0, ktaScale1) * mlx90640.offset[p];
        }

    float temp = 0;
    for (int i = 0; i < 768; i++)
        if (fabs(scratchData[i]) > temp) temp = fabs(scratchData[i]);

    ktaScale1 = 0;
    if (temp == 0) temp = 1;
    while (temp < 64) { temp *= 2; ktaScale1++; }

    for (int i = 0; i < 768; i++)
    {
        float t = scratchData[i] * pow(2.0, ktaScale1);
        mlx90640.kta[i] = (int8_t)(t < 0 ? t - 0.5f : t + 0.5f);
    }
    mlx90640.ktaScale = ktaScale1;
}

static void ExtractKvPixelParameters(uint16_t *eeData)
{
    int8_t  KvT[4];
    uint8_t kvScale = (eeData[56] & 0x0F00) >> 8;

    int8_t KvRoCo = (eeData[52] & 0xF000) >> 12; if (KvRoCo > 7) KvRoCo -= 16; KvT[0] = KvRoCo;
    int8_t KvReCo = (eeData[52] & 0x0F00) >> 8;  if (KvReCo > 7) KvReCo -= 16; KvT[2] = KvReCo;
    int8_t KvRoCe = (eeData[52] & 0x00F0) >> 4;  if (KvRoCe > 7) KvRoCe -= 16; KvT[1] = KvRoCe;
    int8_t KvReCe = (eeData[52] & 0x000F);        if (KvReCe > 7) KvReCe -= 16; KvT[3] = KvReCe;

    for (int i = 0; i < 24; i++)
        for (int j = 0; j < 32; j++)
        {
            int     p     = 32 * i + j;
            uint8_t split = 2 * (p / 32 - (p / 64) * 2) + p % 2;
            scratchData[p] = KvT[split] / pow(2.0, kvScale) * mlx90640.offset[p];
        }

    float temp = 0;
    for (int i = 0; i < 768; i++)
        if (fabs(scratchData[i]) > temp) temp = fabs(scratchData[i]);

    kvScale = 0;
    if (temp == 0) temp = 1;
    while (temp < 64) { temp *= 2; kvScale++; }

    for (int i = 0; i < 768; i++)
    {
        float t = scratchData[i] * pow(2.0, kvScale);
        mlx90640.kv[i] = (int8_t)(t < 0 ? t - 0.5f : t + 0.5f);
    }
    mlx90640.kvScale = kvScale;
}

static void ExtractCILCParameters(uint16_t *eeData)
{
    mlx90640.calibrationModeEE = ((eeData[10] & 0x0800) >> 4) ^ 0x80;

    float ilChessC[3];
    ilChessC[0] = eeData[53] & 0x003F; if (ilChessC[0] > 31) ilChessC[0] -= 64; ilChessC[0] /= 16.0f;
    ilChessC[1] = (eeData[53] & 0x07C0) >> 6; if (ilChessC[1] > 15) ilChessC[1] -= 32; ilChessC[1] /= 2.0f;
    ilChessC[2] = (eeData[53] & 0xF800) >> 11; if (ilChessC[2] > 15) ilChessC[2] -= 32; ilChessC[2] /= 8.0f;

    mlx90640.ilChessC[0] = ilChessC[0];
    mlx90640.ilChessC[1] = ilChessC[1];
    mlx90640.ilChessC[2] = ilChessC[2];
}

static int CheckAdjacentPixels(uint16_t pix1, uint16_t pix2)
{
    int d = pix1 - pix2;
    if ((d > -34 && d < -30) || (d > -2 && d < 2) || (d > 30 && d < 34))
        return -6;
    return 0;
}

static int ExtractDeviatingPixels(uint16_t *eeData)
{
    uint16_t brokenPixCnt = 0, outlierPixCnt = 0;
    int warn = 0;

    for (int i = 0; i < 5; i++)
    {
        mlx90640.brokenPixels[i]  = 0xFFFF;
        mlx90640.outlierPixels[i] = 0xFFFF;
    }

    for (uint16_t pixCnt = 0; pixCnt < 768 && brokenPixCnt < 5 && outlierPixCnt < 5; pixCnt++)
    {
        if      (eeData[pixCnt + 64] == 0)              mlx90640.brokenPixels [brokenPixCnt++]  = pixCnt;
        else if (eeData[pixCnt + 64] & 0x0001)          mlx90640.outlierPixels[outlierPixCnt++] = pixCnt;
    }

    if      (brokenPixCnt > 4)                          warn = -3;
    else if (outlierPixCnt > 4)                         warn = -4;
    else if (brokenPixCnt + outlierPixCnt > 4)          warn = -5;
    else
    {
        for (uint16_t i = 0; i < brokenPixCnt  && !warn; i++)
            for (uint16_t j = i+1; j < brokenPixCnt; j++)
                if ((warn = CheckAdjacentPixels(mlx90640.brokenPixels[i],  mlx90640.brokenPixels[j])))  return warn;
        for (uint16_t i = 0; i < outlierPixCnt && !warn; i++)
            for (uint16_t j = i+1; j < outlierPixCnt; j++)
                if ((warn = CheckAdjacentPixels(mlx90640.outlierPixels[i], mlx90640.outlierPixels[j]))) return warn;
        for (uint16_t i = 0; i < brokenPixCnt  && !warn; i++)
            for (uint16_t j = 0; j < outlierPixCnt; j++)
                if ((warn = CheckAdjacentPixels(mlx90640.brokenPixels[i],  mlx90640.outlierPixels[j]))) return warn;
    }
    return warn;
}

int MLX90640_ExtractParameters(uint16_t *eeData)
{
    ExtractVDDParameters(eeData);
    ExtractPTATParameters(eeData);
    ExtractGainParameters(eeData);
    ExtractTgcParameters(eeData);
    ExtractResolutionParameters(eeData);
    ExtractKsTaParameters(eeData);
    ExtractKsToParameters(eeData);
    ExtractCPParameters(eeData);
    ExtractAlphaParameters(eeData);
    ExtractOffsetParameters(eeData);
    ExtractKtaPixelParameters(eeData);
    ExtractKvPixelParameters(eeData);
    ExtractCILCParameters(eeData);
    return ExtractDeviatingPixels(eeData);
}

/* ============================================================================
 *  Frame acquisition
 * ========================================================================== */

int MLX90640_GetFrameData(void)
{
    uint16_t statusRegister, controlRegister1;
    uint8_t  cnt = 0;

    /* Wait for new data */
    uint16_t dataReady = 0;
    while (!dataReady)
    {
        if (MLX_ReadReg(0x8000, &statusRegister) != HAL_OK)
        {
            HAL_UART_Transmit(&huart2, ErrorMsgRead, sizeof(ErrorMsgRead), 10000);
            break;
        }
        dataReady = statusRegister & 0x0008;
        HAL_Delay(5);
    }

    while (dataReady && cnt < 5)
    {
        if (MLX_WriteReg(0x8000, 0x0030) != HAL_OK)
        {
            HAL_UART_Transmit(&huart2, ErrorMsgWrite, sizeof(ErrorMsgWrite), 10000);
            break;
        }
        if (MLX_ReadBlock(0x0400, 832, frameData) != HAL_OK)
        {
            HAL_UART_Transmit(&huart2, ErrorMsgRead, sizeof(ErrorMsgRead), 10000);
            break;
        }
        if (MLX_ReadReg(0x8000, &statusRegister) != HAL_OK)
        {
            HAL_UART_Transmit(&huart2, ErrorMsgRead, sizeof(ErrorMsgRead), 10000);
            break;
        }
        dataReady = statusRegister & 0x0008;
        cnt++;
    }

    if (cnt > 4) return -8;

    if (MLX_ReadReg(0x800D, &controlRegister1) != HAL_OK)
        HAL_UART_Transmit(&huart2, ErrorMsgRead, sizeof(ErrorMsgRead), 10000);

    frameData[832] = controlRegister1;
    frameData[833] = statusRegister & 0x0001;
    return frameData[833];
}

float MLX90640_GetVdd(void)
{
    float vdd = frameData[810];
    if (vdd > 32767) vdd -= 65536;

    int   resolutionRAM = (frameData[832] & 0x0C00) >> 10;
    float resCorr       = pow(2.0, mlx90640.resolutionEE) / pow(2.0, resolutionRAM);
    return (resCorr * vdd - mlx90640.vdd25) / mlx90640.kVdd + 3.3f;
}

float MLX90640_GetTa(void)
{
    float vdd  = MLX90640_GetVdd();
    float ptat = frameData[800]; if (ptat > 32767) ptat -= 65536;
    float ptatArt = frameData[768]; if (ptatArt > 32767) ptatArt -= 65536;

    ptatArt = (ptat / (ptat * mlx90640.alphaPTAT + ptatArt)) * pow(2.0, 18);
    return (ptatArt / (1 + mlx90640.KvPTAT * (vdd - 3.3f)) - mlx90640.vPTAT25)
           / mlx90640.KtPTAT + 25.0f;
}

void MLX90640_CalculateTo(float emissivity, float tr, float *result)
{
    float vdd = MLX90640_GetVdd();
    float ta  = MLX90640_GetTa();

    float ta4 = (ta + 273.15f); ta4 = ta4*ta4; ta4 = ta4*ta4;
    float tr4 = (tr + 273.15f); tr4 = tr4*tr4; tr4 = tr4*tr4;
    float taTr = tr4 - (tr4 - ta4) / emissivity;

    float ktaScale   = pow(2.0, mlx90640.ktaScale);
    float kvScale    = pow(2.0, mlx90640.kvScale);
    float alphaScale = pow(2.0, mlx90640.alphaScale);

    float alphaCorrR[4];
    alphaCorrR[0] = 1.0f / (1 + mlx90640.ksTo[0] * 40);
    alphaCorrR[1] = 1.0f;
    alphaCorrR[2] = 1 + mlx90640.ksTo[1] * mlx90640.ct[2];
    alphaCorrR[3] = alphaCorrR[2] * (1 + mlx90640.ksTo[2] * (mlx90640.ct[3] - mlx90640.ct[2]));

    float gain = frameData[778]; if (gain > 32767) gain -= 65536;
    gain = mlx90640.gainEE / gain;

    uint8_t mode    = (frameData[832] & 0x1000) >> 5;
    uint16_t subPage = frameData[833];

    float irDataCP[2];
    irDataCP[0] = frameData[776]; if (irDataCP[0] > 32767) irDataCP[0] -= 65536; irDataCP[0] *= gain;
    irDataCP[1] = frameData[808]; if (irDataCP[1] > 32767) irDataCP[1] -= 65536; irDataCP[1] *= gain;

    irDataCP[0] -= mlx90640.cpOffset[0] * (1 + mlx90640.cpKta*(ta-25)) * (1 + mlx90640.cpKv*(vdd-3.3f));
    if (mode == mlx90640.calibrationModeEE)
        irDataCP[1] -= mlx90640.cpOffset[1] * (1 + mlx90640.cpKta*(ta-25)) * (1 + mlx90640.cpKv*(vdd-3.3f));
    else
        irDataCP[1] -= (mlx90640.cpOffset[1] + mlx90640.ilChessC[0]) * (1 + mlx90640.cpKta*(ta-25)) * (1 + mlx90640.cpKv*(vdd-3.3f));

    for (int px = 0; px < 768; px++)
    {
        int8_t ilPattern       = px / 32 - (px / 64) * 2;
        int8_t chessPattern    = ilPattern ^ (px - (px / 2) * 2);
        int8_t convPattern     = ((px+2)/4 - (px+3)/4 + (px+1)/4 - px/4) * (1 - 2*ilPattern);
        int8_t pattern         = (mode == 0) ? ilPattern : chessPattern;

        if (pattern != (int8_t)frameData[833]) continue;

        float irData = frameData[px]; if (irData > 32767) irData -= 65536;
        irData *= gain;

        float kta = mlx90640.kta[px] / ktaScale;
        float kv  = mlx90640.kv[px]  / kvScale;
        irData -= mlx90640.offset[px] * (1 + kta*(ta-25)) * (1 + kv*(vdd-3.3f));

        if (mode != mlx90640.calibrationModeEE)
            irData += mlx90640.ilChessC[2]*(2*ilPattern-1) - mlx90640.ilChessC[1]*convPattern;

        irData -= mlx90640.tgc * irDataCP[subPage];
        irData /= emissivity;

        float alphaComp = SCALEALPHA * alphaScale / mlx90640.alpha[px];
        alphaComp *= (1 + mlx90640.KsTa*(ta-25));

        float Sx = alphaComp*alphaComp*alphaComp*(irData + alphaComp*taTr);
        Sx = sqrt(sqrt(Sx)) * mlx90640.ksTo[1];

        float To = sqrt(sqrt(irData / (alphaComp*(1 - mlx90640.ksTo[1]*273.15f) + Sx) + taTr)) - 273.15f;

        int8_t range;
        if      (To < mlx90640.ct[1]) range = 0;
        else if (To < mlx90640.ct[2]) range = 1;
        else if (To < mlx90640.ct[3]) range = 2;
        else                           range = 3;

        To = sqrt(sqrt(irData / (alphaComp * alphaCorrR[range] *
             (1 + mlx90640.ksTo[range]*(To - mlx90640.ct[range]))) + taTr)) - 273.15f;

        result[px] = To;
    }
}

int MLX90640_getFrame(void)
{
    float emissivity = 0.95f;
    float tr = 23.15f;
    int   status;

    for (uint8_t page = 0; page < 2; page++)
    {
        status = MLX90640_GetFrameData();
        if (status < 0) return status;

        float ta = MLX90640_GetTa();
        tr = ta - OPENAIR_TA_SHIFT;
        MLX90640_CalculateTo(emissivity, tr, tempBuf);
    }

    for (int i = 0; i < 24 * 32; i++)
        frameBuf[i] = f32_to_f16(tempBuf[i]);

    return 0;
}

int MLX90640_getRawFrame(void)
{
    int status;
    for (uint8_t page = 0; page < 2; page++)
    {
        status = MLX90640_GetFrameData();
        if (status < 0) return status;
    }
    return 0;
}

/* ============================================================================
 *  Sensor initialisation
 * ========================================================================== */

void MLX90640_InitSensor(uint8_t debug)
{
    MLX_ReadBlock(0x2400, 832, eepromData);

    if (debug) HAL_UART_Transmit(&huart2, ModeSetMsg, sizeof(ModeSetMsg), 10000);

    uint16_t cr1;
    if (MLX_ReadReg(0x800D, &cr1) != HAL_OK)
        HAL_UART_Transmit(&huart2, ErrorMsgRead, sizeof(ErrorMsgRead), 10000);

    /* Chess mode */
    if (MLX_WriteReg(0x800D, cr1 | 0x1000) != HAL_OK)
        HAL_UART_Transmit(&huart2, ErrorMsgWrite, sizeof(ErrorMsgWrite), 10000);

    MLX_ReadReg(0x800D, &cr1);
    if (debug && ((cr1 & 0x1000) >> 12))
        HAL_UART_Transmit(&huart2, ChessMsg, sizeof(ChessMsg), 10000);
    if (debug) HAL_UART_Transmit(&huart2, ModeSetMsg, sizeof(ModeSetMsg), 10000);

    /* 18-bit resolution */
    if (debug) HAL_UART_Transmit(&huart2, ResolutionSetMsg, sizeof(ResolutionSetMsg), 10000);
    MLX_ReadReg(0x800D, &cr1);
    uint16_t val = (cr1 & 0xF3FF) | ((2u & 0x03) << 10);   // resolution = 2 → 18-bit
    if (MLX_WriteReg(0x800D, val) != HAL_OK)
        HAL_UART_Transmit(&huart2, ErrorMsgWrite, sizeof(ErrorMsgWrite), 10000);
    if (debug) HAL_UART_Transmit(&huart2, ResolutionSetMsg, sizeof(ResolutionSetMsg), 10000);

    /* 4 Hz frame rate */
    if (debug) HAL_UART_Transmit(&huart2, FrameRateSetMsg, sizeof(FrameRateSetMsg), 10000);
    MLX_ReadReg(0x800D, &cr1);
    val = (cr1 & 0xFC7F) | ((4u & 0x07) << 7);             // refreshRate = 4 → 4 Hz
    if (MLX_WriteReg(0x800D, val) != HAL_OK)
        HAL_UART_Transmit(&huart2, ErrorMsgWrite, sizeof(ErrorMsgWrite), 10000);

    if (debug)
    {
        MLX_ReadReg(0x800D, &cr1);
        int rate = (cr1 & 0x0380) >> 7;
        if (rate == 2) { uint8_t m[] = "RATE 2 Hz\n"; HAL_UART_Transmit(&huart2, m, sizeof(m), 10000); }
        if (rate == 4) { uint8_t m[] = "RATE 4 Hz\n"; HAL_UART_Transmit(&huart2, m, sizeof(m), 10000); }
        HAL_UART_Transmit(&huart2, FrameRateSetMsg, sizeof(FrameRateSetMsg), 10000);
    }
}

/* ============================================================================
 *  Diagnostics
 * ========================================================================== */

void MLX90640_diagnostic_test(void)
{
    char buf[100];
    uint16_t testValue;

    HAL_Delay(100);
    HAL_UART_Transmit(&huart2, StartMSG, sizeof(StartMSG), 10000);

    for (uint8_t i = 1; i < 128; i++)
    {
        if (HAL_I2C_IsDeviceReady(thermal_i2c, (uint16_t)(i << 1), 3, 5) != HAL_OK)
            HAL_UART_Transmit(&huart2, Space, sizeof(Space), 10000);
        else
        {
            sprintf(Buffer, "0x%X", i);
            HAL_UART_Transmit(&huart2, Buffer, sizeof(Buffer), 10000);
        }
    }
    HAL_UART_Transmit(&huart2, EndMSG, sizeof(EndMSG), 10000);
    HAL_Delay(100);

    uint16_t serialNo;
    MLX_ReadReg(0x2407, &serialNo);
    char serialNoBuf[6];
    itoa(serialNo, serialNoBuf, 16);
    serialNoBuf[5] = '\n';
    HAL_UART_Transmit(&huart2, (uint8_t *)serialNoBuf, sizeof(serialNoBuf), 10000);

    HAL_StatusTypeDef st;
    st = MLX_ReadReg(0x2400, &testValue);
    sprintf(buf, "EEPROM[0x2400]: 0x%04X, Status: %d\n", testValue, st);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 10000);

    st = MLX_ReadReg(0x8000, &testValue);
    sprintf(buf, "Status[0x8000]: 0x%04X, Status: %d\n", testValue, st);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 10000);

    st = MLX_ReadReg(0x0400, &testValue);
    sprintf(buf, "Frame[0x0400]: 0x%04X, Status: %d\n", testValue, st);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 10000);

    uint16_t testBlock[10];
    st = MLX_ReadBlock(0x2400, 10, testBlock);
    sprintf(buf, "Block read status: %d\n", st);
    HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 10000);
    for (int i = 0; i < 10; i++)
    {
        sprintf(buf, "  [%d]: 0x%04X\n", i, testBlock[i]);
        HAL_UART_Transmit(&huart2, (uint8_t *)buf, strlen(buf), 10000);
    }
}

void MLX90640_Dump_EEPROM(void)
{
    char regBuf[32] = {0};
    for (int i = 0; i < 832; i++)
    {
        sprintf(regBuf, "0x%04X\n", eepromData[i]);
        HAL_UART_Transmit(&huart2, regBuf, sizeof(regBuf), 10000);
    }
}

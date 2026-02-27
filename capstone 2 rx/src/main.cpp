#include <Arduino.h>
#include <SX126x-RAK4630.h>
#include <SPI.h>

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr);
void OnRxTimeout(void);
void OnRxError(void);

#define RF_FREQUENCY        915000000
#define TX_OUTPUT_POWER     22
#define LORA_BANDWIDTH      0
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE     1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define RX_TIMEOUT_VALUE    3000

static RadioEvents_t RadioEvents;
static uint8_t RcvBuffer[200];

void setup()
{
  Serial.begin(115200);
  while (!Serial) delay(10);
  Serial.println("Step 1: Serial OK");

  lora_rak4630_init();
  Serial.println("Step 2: LoRa init OK");

  RadioEvents.TxDone    = NULL;
  RadioEvents.RxDone    = OnRxDone;
  RadioEvents.TxTimeout = NULL;
  RadioEvents.RxTimeout = OnRxTimeout;
  RadioEvents.RxError   = OnRxError;
  RadioEvents.CadDone   = NULL;

  Radio.Init(&RadioEvents);
  Serial.println("Step 3: Radio init OK");

  Radio.SetChannel(RF_FREQUENCY);
  Serial.println("Step 4: Channel set OK");

  Radio.SetRxConfig(MODEM_LORA, LORA_BANDWIDTH, LORA_SPREADING_FACTOR,
                    LORA_CODINGRATE, 0, LORA_PREAMBLE_LENGTH,
                    LORA_SYMBOL_TIMEOUT, LORA_FIX_LENGTH_PAYLOAD_ON,
                    0, true, 0, 0, LORA_IQ_INVERSION_ON, true);
  Serial.println("Step 5: RX config OK");

  Serial1.begin(115200);
  delay(1000);
  Serial.println("Step 6: Serial1 OK - listening for LoRa...");

  Radio.Rx(0);
  Serial.println("Step 7: Radio in continuous RX mode");
}

void loop()
{
  Radio.IrqProcess(); // must be called to handle LoRa interrupts
}

void OnRxDone(uint8_t *payload, uint16_t size, int16_t rssi, int8_t snr)
{
  Serial.printf("Step 8: LoRa packet received! %d bytes, RSSI=%d dBm, SNR=%d\n", size, rssi, snr);

  memcpy(RcvBuffer, payload, size);
  RcvBuffer[size] = '\0';

  // Print received bytes to USB debug terminal
  Serial.print("Data: ");
  for (int i = 0; i < size; i++) Serial.print((char)RcvBuffer[i]);
  Serial.println();

  // Forward to STM32 over UART
  Serial1.write(RcvBuffer, size);
  Serial1.print("\r\n");
  Serial.println("Step 9: Forwarded to STM32 over Serial1");

  Radio.Rx(0); // re-enter continuous receive mode
}

void OnRxTimeout(void)
{
  Serial.println("RxTimeout");
  Radio.Rx(0);
}

void OnRxError(void)
{
  Serial.println("RxError");
  Radio.Rx(0);
}
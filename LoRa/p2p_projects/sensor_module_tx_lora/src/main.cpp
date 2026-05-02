#include <Arduino.h>
#include <SX126x-RAK4630.h>
#include <SPI.h>

void OnTxDone(void);
void OnTxTimeout(void);

#define RF_FREQUENCY 915000000
#define TX_OUTPUT_POWER 22
#define LORA_BANDWIDTH 0
#define LORA_SPREADING_FACTOR 7
#define LORA_CODINGRATE 1
#define LORA_PREAMBLE_LENGTH 8
#define LORA_SYMBOL_TIMEOUT 0
#define LORA_FIX_LENGTH_PAYLOAD_ON false
#define LORA_IQ_INVERSION_ON false
#define TX_TIMEOUT_VALUE 3000

static RadioEvents_t RadioEvents;
static uint8_t TxdBuffer[215];
static bool txBusy = false;

void setup()
{
  Serial.begin(115200);
  // while (!Serial) delay(10); // wait for USB serial to connect
  if (Serial)
    Serial.println("Step 1: Serial OK");

  lora_rak4630_init();
  if (Serial)
    Serial.println("Step 2: LoRa init OK");

  RadioEvents.TxDone = OnTxDone;
  RadioEvents.RxDone = NULL;
  RadioEvents.TxTimeout = OnTxTimeout;
  RadioEvents.RxTimeout = NULL;
  RadioEvents.RxError = NULL;
  RadioEvents.CadDone = NULL;

  Radio.Init(&RadioEvents);
  if (Serial)
    Serial.println("Step 3: Radio init OK");

  Radio.SetChannel(RF_FREQUENCY);
  if (Serial)
    Serial.println("Step 4: Channel set OK");

  Radio.SetTxConfig(MODEM_LORA, TX_OUTPUT_POWER, 0, LORA_BANDWIDTH,
                    LORA_SPREADING_FACTOR, LORA_CODINGRATE,
                    LORA_PREAMBLE_LENGTH, LORA_FIX_LENGTH_PAYLOAD_ON,
                    true, 0, 0, LORA_IQ_INVERSION_ON, TX_TIMEOUT_VALUE);
  if (Serial)
    Serial.println("Step 5: TX config OK");

  Serial1.begin(115200);
  delay(1000);
  if (Serial)
    Serial.println("Step 6: Serial1 OK - ready for UART data");
}

void loop()
{

  Radio.IrqProcess(); // must be called to handle LoRa interrupts

  uint8_t len = 0;
  memset(TxdBuffer, 0, sizeof(TxdBuffer));

  // while (!Serial1.available())
  // {
  //   delay(1);
  // }

  // if (Serial)
  //   Serial.println("Step 7: Data detected on Serial1!");

  uint32_t timeout = millis() + 100;
  while (millis() < timeout && len < sizeof(TxdBuffer) - 1)
  {
    if (Serial1.available())
    {
      char c = Serial1.read();
      // if (c == '\n')
      //   break;
      // if (c == '\r')
      //   continue;
      TxdBuffer[len++] = (uint8_t)c;
      timeout = millis() + 100;
    }
  }

  if (Serial && len > 0)
  {
    Serial.printf("Step 8: Read %d bytes: ", len);
    for (int i = 0; i < len; i++)
      Serial.print((char)TxdBuffer[i]);
    Serial.println();
  }

  if (len > 0 && !txBusy)
  {
    txBusy = true;
    if (Serial)
      Serial.println("Step 9: Sending over LoRa...");
    Radio.Send(TxdBuffer, len);
  }
}

void OnTxDone(void)
{
  if (Serial)
    Serial.println("Step 10: OnTxDone");
  txBusy = false;
}

void OnTxTimeout(void)
{
  if (Serial)
    Serial.println("OnTxTimeout!");
  txBusy = false;
}
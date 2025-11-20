#include <SPI.h>
#include <SD.h>
#include <LoRa.h>

#define LORA_SS_PIN   10
#define LORA_RST_PIN   9
#define LORA_DIO0_PIN  2
#define SD_CS_PIN      4

const long    LORA_FREQUENCY   = 868E6;
const uint8_t LORA_TX_POWER_DB = 14;

const uint8_t NODE_ID         = 4;
const uint8_t SOURCE_NODE_ID  = 1;
const uint8_t EXPECTED_HOP    = 3;
const char   *LOG_FILE        = "node4_log.csv";

const uint8_t TEST_COUNT = 4;
const uint8_t SF_LIST[TEST_COUNT] = {7, 7,12,12};
const uint8_t CR_LIST[TEST_COUNT] = {5, 8, 5, 8};
const char*   TEST_LABELS[TEST_COUNT] = {"SF7CR5","SF7CR8","SF12CR5","SF12CR8"};

struct PacketFields {
  char kind[8];
  char test[16];
  char currentTest[16];
  char nextTest[16];
  char count[16];
  char src[8];
  char hop[8];
  char msg[96];
};

bool sdReady = false;
uint32_t rxCount = 0;
unsigned long lastHeartbeat = 0;
uint8_t currentTestIdx = 0;

void applyTestConfig(uint8_t idx)
{
  LoRa.setSpreadingFactor(SF_LIST[idx]);
  LoRa.setCodingRate4(CR_LIST[idx]);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setPreambleLength(12);
  LoRa.setSyncWord(0x34);
  LoRa.enableCrc();
  LoRa.setTxPower(LORA_TX_POWER_DB);
  currentTestIdx = idx;
  Serial.print("🔧 Node4 radio -> ");
  Serial.println(TEST_LABELS[currentTestIdx]);
}

bool readField(const char* payload, const char* key, char* out, size_t outLen)
{
  char pattern[18];
  snprintf(pattern, sizeof(pattern), "%s=", key);
  const char* start = strstr(payload, pattern);
  if (!start) return false;
  start += strlen(pattern);
  const char* end = strchr(start, ';');
  size_t len = end ? (size_t)(end - start) : strlen(start);
  if (len >= outLen) len = outLen - 1;
  strncpy(out, start, len);
  out[len] = '\0';
  return true;
}

bool parsePacketFields(const char* payload, PacketFields& fields)
{
  memset(&fields, 0, sizeof(fields));
  if (!readField(payload, "KIND", fields.kind, sizeof(fields.kind))) return false;
  readField(payload, "TEST", fields.test, sizeof(fields.test));
  readField(payload, "CURRENT", fields.currentTest, sizeof(fields.currentTest));
  readField(payload, "NEXT", fields.nextTest, sizeof(fields.nextTest));
  readField(payload, "COUNT", fields.count, sizeof(fields.count));
  if (!readField(payload, "SRC", fields.src, sizeof(fields.src))) return false;
  if (!readField(payload, "HOP", fields.hop, sizeof(fields.hop))) return false;
  readField(payload, "MSG", fields.msg, sizeof(fields.msg));
  return true;
}

int testIndexFromLabel(const char* label)
{
  for (uint8_t i = 0; i < TEST_COUNT; ++i)
  {
    if (strcmp(label, TEST_LABELS[i]) == 0) return i;
  }
  return -1;
}

void logLine(const char *text)
{
  if (!sdReady) return;
  digitalWrite(LORA_SS_PIN, HIGH);
  File f = SD.open(LOG_FILE, FILE_WRITE);
  if (f)
  {
    f.println(text);
    f.close();
  }
  else
  {
    Serial.println("⚠️ Node4: SD write failed");
  }
}

void printHeader()
{
  Serial.println("time_ms,event,rxCount,test,rssi,snr,sf,cr,message");
  if (sdReady)
  {
    logLine("time_ms,event,rxCount,test,rssi,snr,sf,cr,message");
  }
}

void handleControl(const PacketFields& fields)
{
  int nextIdx = testIndexFromLabel(fields.nextTest);
  if (nextIdx >= 0)
  {
    applyTestConfig(nextIdx);
  }

  Serial.print("📡 Node4 received CTRL -> ");
  Serial.println(fields.nextTest);
}

void handleData(const PacketFields& fields, int rssi, float snr)
{
  int src = atoi(fields.src);
  int hop = atoi(fields.hop);
  if (src != SOURCE_NODE_ID || hop != EXPECTED_HOP)
  {
    Serial.println("⚠️ Node4: unexpected SRC/HOP");
    return;
  }

  int testIdx = testIndexFromLabel(fields.test);
  if (testIdx >= 0 && testIdx != currentTestIdx)
  {
    applyTestConfig(testIdx);
  }

  rxCount++;

  Serial.print("🏁 Node4 RX #");
  Serial.print(rxCount);
  Serial.print(" | Test ");
  Serial.print(fields.test);
  Serial.print(" | RSSI:");
  Serial.print(rssi);
  Serial.print(" SNR:");
  Serial.println(snr, 2);

  char logEntry[256];
  snprintf(logEntry, sizeof(logEntry),
           "%lu,RX,%lu,%s,%d,%.2f,%u,%u,%s",
           (unsigned long)millis(),
           (unsigned long)rxCount,
           fields.test,
           rssi,
           snr,
           SF_LIST[currentTestIdx],
           CR_LIST[currentTestIdx],
           fields.msg);
  logLine(logEntry);
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LORA_SS_PIN, OUTPUT);
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(LORA_SS_PIN, HIGH);
  digitalWrite(SD_CS_PIN, HIGH);

  Serial.begin(115200);
  while (!Serial && millis() < 2000) { }
  Serial.println("\n🔌 Node 4 (Final Receiver) booting...");

  for (int attempt = 0; attempt < 5 && !sdReady; ++attempt)
  {
    sdReady = SD.begin(SD_CS_PIN);
    if (!sdReady)
    {
      Serial.println("⚠️ Node4: SD init failed, retrying...");
      delay(200);
    }
  }
  Serial.println(sdReady ? "✅ SD card ready." : "❌ SD card missing!");

  LoRa.setPins(LORA_SS_PIN, LORA_RST_PIN, LORA_DIO0_PIN);
  if (!LoRa.begin(LORA_FREQUENCY))
  {
    Serial.println("❌ LoRa init failed. Check wiring.");
    while (true) { delay(10); }
  }
  applyTestConfig(0);
  LoRa.receive();
  Serial.println("✅ LoRa listening for Node 3.");

  printHeader();
}

void loop()
{
  int packetSize = LoRa.parsePacket();
  if (packetSize > 0)
  {
    char payload[256];
    int idx = 0;
    while (LoRa.available() && idx < (int)sizeof(payload) - 1)
    {
      payload[idx++] = (char)LoRa.read();
    }
    payload[idx] = '\0';

    PacketFields fields;
    if (!parsePacketFields(payload, fields))
    {
      Serial.println("⚠️ Node4: failed to parse packet");
      LoRa.receive();
      return;
    }

    int rssi = LoRa.packetRssi();
    float snr = LoRa.packetSnr();

    if (strcmp(fields.kind, "CTRL") == 0)
    {
      handleControl(fields);
    }
    else
    {
      handleData(fields, rssi, snr);
    }
  }

  if (millis() - lastHeartbeat > 60000UL)
  {
    lastHeartbeat = millis();
    Serial.print("ℹ️ Node4 heartbeat | RX:");
    Serial.print(rxCount);
    Serial.print(" | Test ");
    Serial.println(TEST_LABELS[currentTestIdx]);
  }
}


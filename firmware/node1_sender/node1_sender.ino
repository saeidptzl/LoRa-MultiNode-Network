
#include <SPI.h>
#include <SD.h>
#include <LoRa.h>

// ------------------------------------------------------------------
// Hardware configuration (adjust pin numbers to match your wiring)
// ------------------------------------------------------------------
#define LORA_SS_PIN   10   // LoRa module NSS / CS
#define LORA_RST_PIN   9   // LoRa module RESET
#define LORA_DIO0_PIN  2   // LoRa module DIO0 (RxDone)
#define SD_CS_PIN      4   // microSD Chip Select

// ------------------------------------------------------------------
// LoRa & application parameters
// ------------------------------------------------------------------
const long    LORA_FREQUENCY   = 868E6;  // Hz
const uint8_t LORA_SF          = 12;     // spreading factor
const uint8_t LORA_CR          = 5;      // coding rate denominator
const uint8_t LORA_TX_POWER_DB = 14;     // dBm
const uint32_t SEND_INTERVAL_MS = 20000; // 20 seconds between packets

const uint8_t NODE_ID = 1;               // Node 1 -> origin/sender
const char    *MESSAGE_BODY = "PTZL is working on it";
const char    *LOG_FILE = "node1_log.csv";

const uint8_t TEST_COUNT = 4;
const uint8_t SF_LIST[TEST_COUNT] = {7, 7,12,12};
const uint8_t CR_LIST[TEST_COUNT] = {5, 8, 5, 8}; // LoRa uses denominator (4/5..4/8)
const char*   TEST_LABELS[TEST_COUNT] = {"SF7CR5","SF7CR8","SF12CR5","SF12CR8"};
const uint16_t PACKETS_PER_TEST = 100;
const unsigned long MODE_SWITCH_DELAY_MS = 3000;

// ------------------------------------------------------------------
// State
// ------------------------------------------------------------------
bool sdReady = false;
uint32_t packetCount = 0;
unsigned long lastSendMs = 0;
uint8_t currentTest = 0;
uint16_t packetsThisTest = 0;

// ------------------------------------------------------------------
// Helpers
// ------------------------------------------------------------------
void blinkLed()
{
  digitalWrite(LED_BUILTIN, HIGH);
  delay(40);
  digitalWrite(LED_BUILTIN, LOW);
}

void applyTestConfig(uint8_t idx)
{
  LoRa.setSpreadingFactor(SF_LIST[idx]);
  LoRa.setCodingRate4(CR_LIST[idx]);
  LoRa.setSignalBandwidth(125E3);  // 125 kHz per requirements
  LoRa.setPreambleLength(12);
  LoRa.setSyncWord(0x34);
  LoRa.enableCrc();
  LoRa.setTxPower(LORA_TX_POWER_DB);
}

void logLine(const char *text)
{
  if (!sdReady) return;

  digitalWrite(LORA_SS_PIN, HIGH);  // release LoRa so SD can use SPI
  File f = SD.open(LOG_FILE, FILE_WRITE);
  if (f)
  {
    f.println(text);
    f.close();
  }
  else
  {
    Serial.println("⚠️ Node1: failed to write SD log");
  }
}

void sendControlFrame(uint8_t nextTest)
{
  char payload[160];
  snprintf(payload, sizeof(payload),
           "KIND=CTRL;CURRENT=%s;NEXT=%s;SRC=%u;HOP=%u;MSG=MODE_ADVANCE",
           TEST_LABELS[currentTest],
           TEST_LABELS[nextTest],
           NODE_ID,
           NODE_ID);

  LoRa.beginPacket();
  LoRa.print(payload);
  LoRa.endPacket();

  Serial.print("📡 Control sent -> next test ");
  Serial.println(TEST_LABELS[nextTest]);

  char logEntry[196];
  snprintf(logEntry, sizeof(logEntry),
           "%lu,CTRL,%u,%s,N/A,N/A,%u,%u",
           (unsigned long)millis(),
           NODE_ID,
           payload,
           SF_LIST[currentTest],
           CR_LIST[currentTest]);
  logLine(logEntry);
}

void advanceTest()
{
  uint8_t nextTest = (currentTest + 1) % TEST_COUNT;
  sendControlFrame(nextTest);
  delay(MODE_SWITCH_DELAY_MS);
  currentTest = nextTest;
  packetsThisTest = 0;
  applyTestConfig(currentTest);
  Serial.print("🚀 Switched to test ");
  Serial.println(TEST_LABELS[currentTest]);
}

void printHeaderOnce()
{
  Serial.println("time_ms,dir,count,msg,rssi,snr,sf,cr");
  if (sdReady)
  {
    logLine("time_ms,dir,count,msg,rssi,snr,sf,cr");
  }
}

// ------------------------------------------------------------------
// Setup
// ------------------------------------------------------------------
void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(LORA_SS_PIN, OUTPUT);
  pinMode(SD_CS_PIN, OUTPUT);
  digitalWrite(LORA_SS_PIN, HIGH);
  digitalWrite(SD_CS_PIN, HIGH);

  Serial.begin(115200);
  while (!Serial && millis() < 2000) { /* wait for USB if needed */ }
  Serial.println("\n🔌 Node 1 (Sender) booting...");

  // Initialize SD (multiple retries helps when powering from battery)
  for (int attempt = 0; attempt < 5 && !sdReady; ++attempt)
  {
    sdReady = SD.begin(SD_CS_PIN);
    if (!sdReady)
    {
      Serial.println("⚠️ Node1: SD init failed, retrying...");
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
  applyTestConfig(currentTest);
  Serial.print("✅ LoRa radio ready (");
  Serial.print(TEST_LABELS[currentTest]);
  Serial.println(").");

  printHeaderOnce();
}

// ------------------------------------------------------------------
// Loop
// ------------------------------------------------------------------
void loop()
{
  if (millis() - lastSendMs < SEND_INTERVAL_MS)
  {
    return;
  }

  lastSendMs = millis();
  packetCount++;

  char payload[192];
  snprintf(payload, sizeof(payload),
           "KIND=DATA;TEST=%s;COUNT=%lu;SRC=%u;HOP=%u;MSG=%s",
           TEST_LABELS[currentTest],
           (unsigned long)packetCount,
           NODE_ID,
           NODE_ID,
           MESSAGE_BODY);

  LoRa.beginPacket();
  LoRa.print(payload);
  LoRa.endPacket();

  blinkLed();

  Serial.print("📤 Node1 DATA ");
  Serial.print(packetsThisTest + 1);
  Serial.print("/");
  Serial.print(PACKETS_PER_TEST);
  Serial.print(" | Test ");
  Serial.print(TEST_LABELS[currentTest]);
  Serial.print(" | Count ");
  Serial.print(packetCount);
  Serial.print(" | SF:");
  Serial.print(SF_LIST[currentTest]);
  Serial.print(" CR:4/");
  Serial.println(CR_LIST[currentTest]);

  char logEntry[196];
  snprintf(logEntry, sizeof(logEntry),
           "%lu,DATA,%lu,%s,N/A,N/A,%u,%u",
           (unsigned long)millis(),
           (unsigned long)packetCount,
           payload,
           SF_LIST[currentTest],
           CR_LIST[currentTest]);
  logLine(logEntry);

  packetsThisTest++;
  if (packetsThisTest >= PACKETS_PER_TEST)
  {
    advanceTest();
  }
}

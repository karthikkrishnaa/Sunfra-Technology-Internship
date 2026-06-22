#include <SPI.h>
#include <LoRa.h>
#include <ModbusMaster.h>
#include <WiFi.h>

// ── LoRa Pin Definitions ──────────────────────────────
#define LORA_SS   5
#define LORA_RST  14
#define LORA_DIO0 12
#define DEVICE_ID "FARM02"
int packetNumber = 0;

String DEVICE_MAC = "";
ModbusMaster node;
uint16_t smitValue = 0;

// ── RS485 Pin Definitions ─────────────────────────────
#define RXD2 16
#define TXD2 17
#define RS485_DE_RE 25
float weightKg = 0;

String getMacAddress() {
  WiFi.mode(WIFI_STA);
  delay(500);
  String mac = WiFi.macAddress();
  mac.replace(":", "-");
  return mac;
}

void preTransmission() {
  digitalWrite(RS485_DE_RE, HIGH);
  delay(2);   // slightly longer guard time, helps with cheap RS485 modules
}

void postTransmission() {
  delay(2);   // let last byte fully clock out before releasing DE
  digitalWrite(RS485_DE_RE, LOW);
}

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== LoRa Transmitter Starting ===");

  DEVICE_MAC = getMacAddress();
  Serial.println("Device MAC  : " + DEVICE_MAC);
  Serial.println("Device ID   : " + String(DEVICE_ID));

  // RS485 / Modbus setup
  pinMode(RS485_DE_RE, OUTPUT);
  digitalWrite(RS485_DE_RE, LOW);   // start in receive mode

  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

  node.begin(1, Serial2);           // SMIT-3016 Slave ID = 1
  node.preTransmission(preTransmission);
  node.postTransmission(postTransmission);

  // LoRa setup — initialize AFTER Modbus/Serial2 to avoid any pin/bus contention
  SPI.begin(18, 19, 23, 5);   // SCK, MISO, MOSI, SS
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init FAILED! Check wiring.");
    while (true) { delay(1000); }   // avoid hard-locking the watchdog
  }

  LoRa.setSpreadingFactor(9);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setTxPower(20);

  Serial.println("LoRa Initialized Successfully!");
  Serial.println("Frequency   : 433 MHz");
  Serial.println("SF          : 9");
  Serial.println("Bandwidth   : 125 kHz");
  Serial.println("Coding Rate : 4/5");
  Serial.println("TX Power    : 20 dBm");
  Serial.println("Waiting to transmit...\n");
}

void loop() {
    // shortened from 5000 — long idle gaps can cause slave timeouts

  node.clearResponseBuffer();
  uint8_t result = node.readHoldingRegisters(0, 2);

  if (result == node.ku8MBSuccess) {
    Serial.println("\n===== SMIT-3016 Registers =====");

    int16_t signedValue = (int16_t)node.getResponseBuffer(0);  // FIX: signed cast
    weightKg = signedValue / 1000.0;

    for (int i = 0; i < 2; i++) {
        Serial.print("Reg ");
        Serial.print(i);
        Serial.print(" = ");
        Serial.println(node.getResponseBuffer(i));
    }

    Serial.print("Weight = ");
    Serial.print(weightKg, 5);
    Serial.println(" kg");

    Serial.println("==============================");

  } else {
    Serial.print("Modbus Error: ");
    Serial.println(result);
    // Note: weightKg is intentionally NOT reset to 0 here, so the LoRa
    // packet still sends the last known good weight instead of garbage.
    // Remove this comment and add "weightKg = 0;" here if you'd rather
    // transmit 0 on every failed read.
  }

  String data = "PKT:" + String(packetNumber)
              + ",ID:" + String(DEVICE_ID)
              + ",MAC:" + DEVICE_MAC
              + ",WT:" + String(weightKg, 5);

  Serial.println("\n=============================");
  Serial.println("Packet No   : " + String(packetNumber));
  Serial.println("Weight (kg) : " + String(weightKg, 5));

  Serial.println("[LORA] Sending via LoRa...");
  LoRa.beginPacket();
  LoRa.print(data);
  LoRa.endPacket();
  Serial.println("[LORA] Packet sent! #" + String(packetNumber));

  packetNumber++;
  delay(7000); 
}
#include <SPI.h>
#include <LoRa.h>
#include <DHT.h>
#include <WiFi.h>
#include <Wire.h>
#include <U8g2lib.h>
//#include <ModbusMaster.h>

// ── LoRa Pin Definitions ──────────────────────────────
#define LORA_SS   5
#define LORA_RST  14
#define LORA_DIO0 12

// ── DHT11 ─────────────────────────────────────────────
#define DHTPIN  26
#define DHTTYPE DHT11

// ── OLED SH1106 1.3" I2C ─────────────────────────────
U8G2_SH1106_128X64_NONAME_F_HW_I2C
  u8g2(U8G2_R0, U8X8_PIN_NONE);

// ── Water Flow Sensor ─────────────────────────────────
#define FLOW_SENSOR_PIN 27
//#define RXD2 16
//#define TXD2 17
//#define RS485_DE_RE 4

// ── Device Identity ───────────────────────────────────
#define DEVICE_ID "FARM01"

// ── Device MAC (auto-read from WiFi chip) ─────────────
String DEVICE_MAC = "";

// ── Globals ───────────────────────────────────────────
DHT dht(DHTPIN, DHTTYPE);
int packetNumber = 0;
//ModbusMaster node;
//uint16_t smitValue = 0;

volatile unsigned long pulseCount = 0;
volatile unsigned long totalPulseCount = 0;

// ── Last sensor readings (for display) ───────────────
float         g_temperature = 0.0;
float         g_humidity    = 0.0;
unsigned long g_pulses      = 0;
float         g_flowRate    = 0.0;
String        g_flowStatus  = "--";

// ── Display Page Control ──────────────────────────────
int           displayPage    = 0;
unsigned long lastPageSwitch = 0;
const long    PAGE_INTERVAL  = 3000;

// ─────────────────────────────────────────────────────
//  Auto-read MAC from WiFi chip (no network needed)
// ─────────────────────────────────────────────────────
String getMacAddress() {
  WiFi.mode(WIFI_STA);
  delay(500);
  String mac = WiFi.macAddress();  // "A4:E5:7C:66:F4:A0"
  mac.replace(":", "-");           // "A4-E5-7C-66-F4-A0"
  return mac;
}

// ─────────────────────────────────────────────────────
//  Interrupt — Flow Sensor Pulse Counter
// ─────────────────────────────────────────────────────
void IRAM_ATTR pulseCounter() {
  pulseCount++;
   totalPulseCount++;
}

// ─────────────────────────────────────────────────────
//  Page 1 — Temperature + Humidity
// ─────────────────────────────────────────────────────
void showDHTPage() {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.drawStr(20, 10, "Temp & Humidity");
  u8g2.setDrawColor(1);
  u8g2.drawHLine(0, 13, 128);

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 27, "Temp :");
  u8g2.setFont(u8g2_font_7x14B_tf);
  u8g2.drawStr(50, 28, (String(g_temperature, 1) + " C").c_str());

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 43, "Hum  :");
  u8g2.setFont(u8g2_font_7x14B_tf);
  u8g2.drawStr(50, 43, (String(g_humidity, 1) + " %").c_str());

  u8g2.drawHLine(0, 47, 128);

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0,  58, ("ID:" + String(DEVICE_ID)).c_str());
  u8g2.drawStr(75, 58, ("P:" + String(packetNumber)).c_str());

  // Page indicator dots
  u8g2.drawDisc(108, 6, 2);    // filled = active
  u8g2.drawCircle(116, 6, 2);
  u8g2.drawCircle(124, 6, 2);

  u8g2.sendBuffer();
}

// ─────────────────────────────────────────────────────
//  Page 2 — Water Flow
// ─────────────────────────────────────────────────────
void showFlowPage() {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.drawStr(25, 10, "Water Flow");
  u8g2.setDrawColor(1);
  u8g2.drawHLine(0, 13, 128);

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 27, "Rate  :");
  u8g2.setFont(u8g2_font_7x14B_tf);
  u8g2.drawStr(50, 28, (String(g_flowRate, 1) + " L/m").c_str());

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 42, "Live:");
  u8g2.setFont(u8g2_font_7x14B_tf);
  u8g2.drawStr(50, 43, String(g_pulses).c_str());

u8g2.drawHLine(0, 47, 128);

u8g2.setFont(u8g2_font_6x10_tf);
u8g2.drawStr(0, 54, ("Total:" + String(totalPulseCount)).c_str());
u8g2.drawStr(0, 63, ("Status: " + g_flowStatus).c_str());

  // Page indicator dots
  u8g2.drawCircle(108, 6, 2);
  u8g2.drawDisc(116, 6, 2);    // filled = active
  u8g2.drawCircle(124, 6, 2);

  u8g2.sendBuffer();
}

// ─────────────────────────────────────────────────────
//  Page 3 — Signal Quality  (TX side: shows TX power + MAC)
// ─────────────────────────────────────────────────────
void showSignalPage() {
  u8g2.clearBuffer();

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.drawStr(22, 10, "TX  Info");
  u8g2.setDrawColor(1);
  u8g2.drawHLine(0, 13, 128);

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 27, "TX Pwr:");
  u8g2.setFont(u8g2_font_7x14B_tf);
  u8g2.drawStr(50, 28, "20 dBm");

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 42, "PKT No:");
  u8g2.setFont(u8g2_font_7x14B_tf);
  u8g2.drawStr(50, 43, String(packetNumber).c_str());

  u8g2.drawHLine(0, 47, 128);


  // Page indicator dots
  u8g2.drawCircle(108, 6, 2);
  u8g2.drawCircle(116, 6, 2);
  u8g2.drawDisc(124, 6, 2);    // filled = active

  u8g2.sendBuffer();
  u8g2.setFont(u8g2_font_5x7_tf);

String mac1 = DEVICE_MAC.substring(0, 8);
String mac2 = DEVICE_MAC.substring(9);

u8g2.drawStr(0, 56, mac1.c_str());
u8g2.drawStr(0, 64, mac2.c_str());
}

// ─────────────────────────────────────────────────────
//  Startup Screen
// ─────────────────────────────────────────────────────
void showStartupScreen() {
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x14B_tf);
  u8g2.drawStr(20, 18, "LoRa TX");
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(10, 32, "DHT11 + Water Flow");
  u8g2.drawStr(20, 44, "433MHz  SF9  4/5");
  u8g2.drawStr(25, 58, "ID: FARM01");
  u8g2.sendBuffer();
  delay(2000);
}

// ─────────────────────────────────────────────────────
//  Sending Screen
// ─────────────────────────────────────────────────────
void showSendingScreen() {
  String pktStr = "PKT #" + String(packetNumber);
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x14B_tf);
  u8g2.drawStr(15, 28, "Sending...");
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(10, 44, pktStr.c_str());
  u8g2.sendBuffer();
}

// ─────────────────────────────────────────────────────
//  Update Display — cycle through pages
// ─────────────────────────────────────────────────────
void updateDisplay() {
  if (displayPage == 0)      showDHTPage();
  else if (displayPage == 1) showFlowPage();
  else if (displayPage == 2) showSignalPage();
  //else                      showSmitPage();
}
/*void preTransmission() 
 { digitalWrite(RS485_DE_RE, HIGH); delay(1); 
 }
void postTransmission() 
{ digitalWrite(RS485_DE_RE, LOW);  delay(3);
 }
//SMIT page
void showSmitPage() {
  Serial.println("DEBUG: showSmitPage() called");  // ADD THIS
  u8g2.clearBuffer();
  
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawBox(0, 0, 128, 12);
  u8g2.setDrawColor(0);
  u8g2.drawStr(30, 10, "SMIT-3016");
  u8g2.setDrawColor(1);
  u8g2.drawHLine(0, 13, 128);

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 27, "Value :");
  u8g2.setFont(u8g2_font_7x14B_tf);
  u8g2.drawStr(50, 28, String(smitValue).c_str());

  u8g2.drawHLine(0, 47, 128);

  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(0, 58, ("Reg: " + String(smitValue)).c_str());

  // Page indicator dots
  u8g2.drawCircle(100, 6, 2);
  u8g2.drawCircle(108, 6, 2);
  u8g2.drawCircle(116, 6, 2);
  u8g2.drawDisc(124, 6, 2);

  u8g2.sendBuffer();
}
*/
// ─────────────────────────────────────────────────────
//  Setup
// ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("=== LoRa Transmitter Starting ===");

  // ── OLED Init ──────────────────────────────────────
  Wire.begin(21, 22);
  u8g2.begin();
  u8g2.setContrast(255);
  showStartupScreen();
  Serial.println("OLED Initialized!");

  // ── Auto-read MAC (no WiFi connection needed) ──────
  DEVICE_MAC = getMacAddress();
  Serial.println("Device MAC  : " + DEVICE_MAC);
  Serial.println("Device ID   : " + String(DEVICE_ID));

  // ── DHT11 Init ─────────────────────────────────────
  dht.begin();

  // ── Flow Sensor Interrupt ──────────────────────────
  pinMode(FLOW_SENSOR_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), pulseCounter, FALLING);
  //MOD-3016
  // RS485 UART
//Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);

/* SMIT-3016 Slave ID = 1
pinMode(RS485_DE_RE, OUTPUT);
digitalWrite(RS485_DE_RE, LOW);
node.begin(1, Serial2);
node.preTransmission(preTransmission);
node.postTransmission(postTransmission);
*/
  // ── Init LoRa ──────────────────────────────────────
  SPI.begin(18, 19, 23, 5);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init FAILED! Check wiring.");
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_7x14B_tf);
    u8g2.drawStr(10, 30, "LoRa FAILED!");
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(5, 50, "Check wiring & reset");
    u8g2.sendBuffer();
    while (true);
  }

  // ── LoRa Parameters ────────────────────────────────
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

  // ── Ready Screen ───────────────────────────────────
  u8g2.clearBuffer();
  u8g2.setFont(u8g2_font_7x14B_tf);
  u8g2.drawStr(10, 25, "LoRa Ready!");
  u8g2.setFont(u8g2_font_6x10_tf);
  u8g2.drawStr(5,  42, "433MHz | SF9 | 20dBm");
  u8g2.drawStr(15, 56, "Reading sensors...");
  u8g2.sendBuffer();
  delay(1500);

  lastPageSwitch = millis();
}

// ─────────────────────────────────────────────────────
//  Loop
// ─────────────────────────────────────────────────────
void loop() {

  // ── Auto Page Switch every 3 seconds ─────────────
  if (millis() - lastPageSwitch >= PAGE_INTERVAL) {
    displayPage = (displayPage + 1) % 3;
    lastPageSwitch = millis();
    updateDisplay();
  }

  // ── Read DHT11 ───────────────────────────────────
  float temperature = dht.readTemperature();
  float humidity    = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("[DHT] Read failed! Check wiring.");
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.drawStr(10, 32, "DHT Read Failed!");
    u8g2.drawStr(10, 46, "Check wiring...");
    u8g2.sendBuffer();
    delay(5000);
    return;
  }

  g_temperature = temperature;
  g_humidity    = humidity;

  // ── Count Flow Pulses for 5 seconds ──────────────
  pulseCount = 0;
unsigned long flowStart = millis();
while (millis() - flowStart < 1000) {  // Reduce to 1 second
  delay(10);
}
 /* uint8_t result = node.readHoldingRegisters(0, 1);

if (result == node.ku8MBSuccess)
{
    smitValue = node.getResponseBuffer(0);

    Serial.print("SMIT Reg0 = ");
    Serial.println(smitValue);
}
else
{
    Serial.print("Modbus Error: ");
    Serial.println(result);
}
*/
  g_pulses     = pulseCount;
  g_flowRate   = g_pulses / 7.5;
  g_flowStatus = (g_pulses > 0) ? "flowing" : "idle";

  packetNumber++;

  // ── Build Data String (MAC auto-included) ─────────
  // Format: PKT:1,TEMP:28.5,HUM:64.0,PULSES:45,FLOW:6.0,STATUS:flowing,ID:FARM01,MAC:A4-E5-7C-66-F4-A0
  String data = "PKT:" + String(packetNumber)
            + ",TEMP:" + String(g_temperature,1)
            + ",HUM:" + String(g_humidity,1)
            //+ ",SMIT:" + String(smitValue)
            + ",PULSES:" + String(g_pulses)
            + ",FLOW:" + String(g_flowRate,1)
            + ",STATUS:" + g_flowStatus
            + ",ID:" + String(DEVICE_ID)
            + ",MAC:" + DEVICE_MAC;

  // ── Print to Serial ───────────────────────────────
  Serial.println("\n=============================");
  Serial.println("Packet No   : " + String(packetNumber));
  Serial.println("Temperature : " + String(g_temperature, 1) + " C");
  Serial.println("Humidity    : " + String(g_humidity, 1)    + " %");
  Serial.println("Pulses      : " + String(g_pulses));
  Serial.println("Total Pulse : " + String(totalPulseCount));
  Serial.println("Flow Rate   : " + String(g_flowRate, 1)    + " L/min");
  Serial.println("Flow Status : " + g_flowStatus);
  //Serial.println("SMIT Value : " + String(smitValue));
  Serial.println("Device ID   : " + String(DEVICE_ID));
  Serial.println("Device MAC  : " + DEVICE_MAC);
  Serial.println("Data String : " + data);
  Serial.println("Packet Size : " + String(data.length())    + " bytes");
  Serial.println("=============================");

  // ── Flash Sending Screen ──────────────────────────
  showSendingScreen();

  // ── Transmit via LoRa ─────────────────────────────
  Serial.println("[LORA] Sending via LoRa...");
  LoRa.beginPacket();
  LoRa.print(data);
  LoRa.endPacket();
  Serial.println("[LORA] Packet sent! #" + String(packetNumber));

  // ── Update display immediately after TX ───────────
  updateDisplay();
  lastPageSwitch = millis();

  // ── Wait 8 seconds before next cycle ─────────────
  unsigned long waitStart = millis();
  while (millis() - waitStart < 8000) {
    if (millis() - lastPageSwitch >= PAGE_INTERVAL) {
      displayPage = (displayPage + 1) % 3;
      lastPageSwitch = millis();
      updateDisplay();
    }
    delay(10000);
  }
}

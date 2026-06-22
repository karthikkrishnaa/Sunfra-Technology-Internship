#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>


// ── LoRa Pin Definitions ──────────────────────────────
#define LORA_SS   5
#define LORA_RST  14
#define LORA_DIO0 2

// ── API Endpoints ─────────────────────────────────────
#define FLOW_API  "https://sunfra.com/farm/sensor/water_flow_meter.php"
#define DHT_API   "https://sunfra.com/farm/sensor/temperature_with_nodemcu.php"
#define SMIT_API  "https://sunfra.com/farm/sensor/indicator_reading.php"

// ── Globals ───────────────────────────────────────────
int    totalPacketsReceived = 0;

String g_pktNumber   = "--";
String g_temperature = "--";
String g_humidity    = "--";
String g_pulses      = "--";
String g_flowRate    = "--";
String g_flowStatus  = "--";
String g_deviceID    = "--";
String g_deviceMAC   = "--";
String g_smitValue = "--";

int    g_rssi = 0;
float  g_snr  = 0.0;
WiFiManager wm;

// ─────────────────────────────────────────────────────
// Parse value from received packet
// ─────────────────────────────────────────────────────
String getValue(String data, String key) {

  int startIndex = data.indexOf(key);

  if (startIndex == -1)
    return "N/A";

  startIndex += key.length();

  int endIndex = data.indexOf(",", startIndex);

  if (endIndex == -1)
    endIndex = data.length();

  return data.substring(startIndex, endIndex);
}

// ─────────────────────────────────────────────────────
// Send DHT Data
// ─────────────────────────────────────────────────────
void sendDHTAPI() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = String(DHT_API) + "?mac_address=" + g_deviceMAC + "&temp=" + g_temperature + "&humidity=" + g_humidity;
  Serial.println("[DHT API URL] " + url);  // ADD THIS
  http.begin(client, url);
  int httpCode = http.GET();
  Serial.println("[DHT API] Code: " + String(httpCode));
  http.end();
}

void sendFlowAPI() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = String(FLOW_API) + "?pulses=" + g_pulses + "&mac=" + g_deviceMAC + "&status=" + g_flowStatus;
  Serial.println("[Flow API URL] " + url);  // ADD THIS
  http.begin(client, url);
  int httpCode = http.GET();
  Serial.println("[Flow API] Code: " + String(httpCode));
  http.end();
}

void SMITdataAPI() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = String(SMIT_API) + "?mac_address=" + g_deviceMAC + "&value=" + g_smitValue;
  Serial.println("[SMIT API URL] " + url);  // ADD THIS
  http.begin(client, url);
  int httpCode = http.GET();
  Serial.println("[SMIT API] Code: " + String(httpCode));
  http.end();
}


// ─────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────
void setup() {

  Serial.begin(115200);
  delay(2000);

  Serial.println("\nStarting WiFi Setup Portal...");

  // Uncomment ONCE ONLY if you want to erase saved credentials
  // wm.resetSettings();

wm.setConfigPortalBlocking(true);
wm.setConfigPortalTimeout(300);
bool res = wm.autoConnect("ESP32_Setup", "12345678");

if (res) {
  Serial.println("\nWiFi Connected!");
  Serial.println("SSID: " + WiFi.SSID());
  Serial.println("IP: " + WiFi.localIP().toString());
  
  // Now start portal in background after connection succeeds
  wm.setConfigPortalBlocking(false);
  wm.startWebPortal();
} else {
  Serial.println("WiFi Connection Failed");
}
  // ── Initialize LoRa ──────────────────────────

  Serial.println("\nInitializing LoRa...");

  LoRa.setPins(
      LORA_SS,
      LORA_RST,
      LORA_DIO0
  );

  if (!LoRa.begin(433E6)) {

    Serial.println("LoRa init FAILED!");

    while (true);
  }

  LoRa.setSpreadingFactor(9);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);

  Serial.println("LoRa Ready!");
  Serial.println("Listening for packets...\n");
  
}

// ─────────────────────────────────────────────────────
// Loop
// ─────────────────────────────────────────────────────
void loop() {
  wm.process();
  //wm.resetSettings();
  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("[WiFi] Connection Lost");

    return;
  }

  int packetSize = LoRa.parsePacket();

  if (packetSize) {

    String received = "";

    while (LoRa.available()) {
      received += (char)LoRa.read();
    }

    g_rssi = LoRa.packetRssi();
    g_snr  = LoRa.packetSnr();

    totalPacketsReceived++;

    g_pktNumber   = getValue(received, "PKT:");
    g_temperature = getValue(received, "TEMP:");
    g_humidity    = getValue(received, "HUM:");
    g_pulses      = getValue(received, "PULSES:");
    g_flowRate    = getValue(received, "FLOW:");
    g_flowStatus  = getValue(received, "STATUS:");
    g_deviceID    = getValue(received, "ID:");
    g_deviceMAC   = getValue(received, "MAC:");
    g_smitValue = getValue(received, "SMIT:");

    Serial.println("\n=============================");
    Serial.println("[RX] Packet Received");
    Serial.println("=============================");

    Serial.println("Raw Data    : " + received);
    Serial.println("Packet No   : " + g_pktNumber);
    Serial.println("Temperature : " + g_temperature);
    Serial.println("Humidity    : " + g_humidity);
    Serial.println("Pulses      : " + g_pulses);
    Serial.println("Flow Rate   : " + g_flowRate);
    Serial.println("Status      : " + g_flowStatus);
    Serial.println("Device ID   : " + g_deviceID);
    Serial.println("Device MAC  : " + g_deviceMAC);
    Serial.println("SMIT Value  : " + g_smitValue);
    Serial.println("RSSI        : " + String(g_rssi));
    Serial.println("SNR         : " + String(g_snr));

    Serial.println("Total RX    : " + String(totalPacketsReceived));

    Serial.println("\nSending Data To APIs...");

    sendDHTAPI();
    delay(500);
    sendFlowAPI();
    delay(500);
    SMITdataAPI();
    Serial.println("API Upload Complete\n");
    delay(5000);
  }
}

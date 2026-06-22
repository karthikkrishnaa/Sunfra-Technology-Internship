#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>

// ── LoRa Pin Definitions ──────────────────────────────
#define LORA_SS   5
#define LORA_RST  14
#define LORA_DIO0 2

// ── API Endpoints ─────────────────────────────────────
#define FLOW_API  "https://sunfra.com/farm/sensor/water_flow_meter.php"
#define DHT_API   "https://sunfra.com/farm/sensor/temperature_with_nodemcu.php"

// ── Globals ───────────────────────────────────────────
WebServer server(80);
WiFiManager wm;


String firstSSID = "";
String firstPass = "";
String currentSSID = "";
String currentPass = "";

int    totalPacketsReceived = 0;
String g_pktNumber   = "--";
String g_temperature = "--";
String g_humidity    = "--";
String g_pulses      = "--";
String g_flowRate    = "--";
String g_flowStatus  = "--";
String g_deviceID    = "--";
String g_deviceMAC   = "--";
int    g_rssi = 0;
float  g_snr  = 0.0;

// ─────────────────────────────────────────────────────
// Save credentials callback
// ─────────────────────────────────────────────────────
void saveCredentials() {
  currentSSID = WiFi.SSID();
  currentPass = wm.getWiFiPass();
  Serial.println("Credentials saved for: " + currentSSID);
}
// ─────────────────────────────────────────────────────
// Parse value from received packet
// ─────────────────────────────────────────────────────
String getValue(String data, String key) {
  int startIndex = data.indexOf(key);
  if (startIndex == -1) return "N/A";
  startIndex += key.length();
  int endIndex = data.indexOf(",", startIndex);
  if (endIndex == -1) endIndex = data.length();
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
  http.begin(client, url);
  int httpCode = http.GET();
  Serial.println("[DHT API] Code: " + String(httpCode));
  http.end();
}

// ─────────────────────────────────────────────────────
// Send Flow Data
// ─────────────────────────────────────────────────────
void sendFlowAPI() {
  if (WiFi.status() != WL_CONNECTED) return;
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  String url = String(FLOW_API) + "?pulses=" + g_pulses + "&mac=" + g_deviceMAC + "&status=" + g_flowStatus;
  http.begin(client, url);
  int httpCode = http.GET();
  Serial.println("[Flow API] Code: " + String(httpCode));
  http.end();
}

// ─────────────────────────────────────────────────────
// Web Page Handler
// ─────────────────────────────────────────────────────
void handleRoot() {
  String html = "<!DOCTYPE html><html><head>";
  html += "<meta charset='UTF-8'>";
  html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
  html += "<title>ESP32 Gateway</title>";
  html += "<style>body{font-family:Arial;padding:40px;text-align:center;background:#f0f0f0;}";
  html += "h1{color:#333;}p{font-size:16px;}";
  html += "button{padding:15px 40px;font-size:16px;margin:10px;color:white;border:none;border-radius:5px;cursor:pointer;}";
  html += ".blue{background:#4a90e2;}.red{background:#ff6b6b;}";
  html += "button:hover{opacity:0.8;}</style></head><body>";
  html += "<h1>ESP32 LoRa Gateway</h1>";
  html += "<p><b>Current WiFi:</b> " + WiFi.SSID() + "</p>";
  html += "<p><b>IP Address:</b> " + WiFi.localIP().toString() + "</p>";
  html += "<p><b>Status:</b> LoRa Running</p><hr>";
  html += "<form action='/configure' method='POST'>";
  html += "<button class='blue' type='submit'>Configure WiFi</button>";
  html += "</form>";
  html += "<form action='/reset' method='POST'>";
  html += "<button class='red' type='submit'>Reset to Previous WiFi</button>";
  html += "</form>";
  html += "</body></html>";
  server.send(200, "text/html", html);
}

void handleConfigure() {
  server.send(200, "text/html", "<html><body><h2>Starting Config Portal...</h2><p>Connect to ESP32_Setup hotspot and visit 192.168.4.1</p></body></html>");
  delay(1000);
  server.stop();
  wm.setConfigPortalBlocking(true);
  wm.startConfigPortal("ESP32_Setup", "12345678");
  String savedFirst = firstSSID;
  String savedFirstPass = firstPass;
  firstSSID = savedFirst;
  firstPass = savedFirstPass;
  currentSSID = WiFi.SSID();
  currentPass = wm.getWiFiPass();
  Serial.println("First WiFi preserved: " + firstSSID);
  Serial.println("Current WiFi: " + currentSSID);
  Serial.println("\nNew WiFi Connected!");
  Serial.println("SSID: " + WiFi.SSID());
  Serial.println("New IP: " + WiFi.localIP().toString());
  server.on("/", handleRoot);
  server.on("/configure", HTTP_POST, handleConfigure);
  server.on("/reset", HTTP_POST, handleReset);
  server.begin();
}
void handleReset() {
  String resetTo = firstSSID;
  String resetPass = firstPass;
  Serial.println("Resetting to: " + resetTo);
  server.send(200, "text/html", "<html><body><h2>Reconnecting to: " + resetTo + "</h2></body></html>");
  delay(1000);
  WiFi.disconnect();
  delay(500);
  WiFi.begin(resetTo.c_str(), resetPass.c_str());
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    attempts++;
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nReconnected to: " + resetTo);
    Serial.println("IP: " + WiFi.localIP().toString());
    server.on("/", handleRoot);
    server.on("/configure", HTTP_POST, handleConfigure);
    server.on("/reset", HTTP_POST, handleReset);
  } else {
    Serial.println("Failed, restarting...");
    ESP.restart();
  }
}

// ─────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\nStarting...");

  // wm.resetSettings(); // Uncomment ONCE to clear saved credentials

  wm.setSaveConfigCallback(saveCredentials);
  wm.setConfigPortalBlocking(true);
  wm.setConfigPortalTimeout(300);
  bool res = wm.autoConnect("ESP32_Setup", "12345678");

  if (res) {
   firstSSID = WiFi.SSID();
  firstPass = wm.getWiFiPass();
    Serial.println("\nWiFi Connected!");
    Serial.println("SSID: " + WiFi.SSID());
    Serial.println("IP: " + WiFi.localIP().toString());
    
  } else {
    Serial.println("WiFi Failed!");
  }

  // ── Initialize LoRa ──────────────────────────
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init FAILED!");
    while (true);
  }
  LoRa.setSpreadingFactor(9);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  Serial.println("LoRa Ready!");

  // ── Web Server ──────────────────────────
  server.on("/", handleRoot);
  server.on("/configure", HTTP_POST, handleConfigure);
  server.on("/reset", HTTP_POST, handleReset);
  server.begin();
  Serial.println("Web server: http://" + WiFi.localIP().toString());
}

// ─────────────────────────────────────────────────────
// Loop
// ─────────────────────────────────────────────────────
void loop() {
  server.handleClient();
  

  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Connection Lost");
    return;
  }

  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    String received = "";
    while (LoRa.available()) received += (char)LoRa.read();

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

    Serial.println("\n=============================");
    Serial.println("[RX] Packet Received");
    Serial.println("=============================");
    Serial.println("Raw Data    : " + received);
    Serial.println("Temperature : " + g_temperature);
    Serial.println("Humidity    : " + g_humidity);
    Serial.println("Pulses      : " + g_pulses);
    Serial.println("Flow Rate   : " + g_flowRate);
    Serial.println("Status      : " + g_flowStatus);
    Serial.println("RSSI        : " + String(g_rssi));
    Serial.println("Total RX    : " + String(totalPacketsReceived));

    sendDHTAPI();
    delay(500);
    sendFlowAPI();
    Serial.println("API Upload Complete\n");
    delay(2000);
  }
}
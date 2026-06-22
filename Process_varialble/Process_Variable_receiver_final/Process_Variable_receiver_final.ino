#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <WebServer.h>
#include "esp_wifi.h"
#include <Preferences.h>
Preferences prefs;


// ── LoRa Pin Definitions ──────────────────────────────
#define LORA_SS   5
#define LORA_RST  14
#define LORA_DIO0 4

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
WebServer server(80);

// ── WiFi Network Tracking ─────────────────────────────
String g_currentSSID = "";
String g_previousSSID = "";
bool g_isFirstConnection = true;

// ── WiFi Array Storage ────────────────────────────────
#define MAX_SAVED_WIFI 2
String savedSSIDs[MAX_SAVED_WIFI] = {"", ""};
String savedPasswords[MAX_SAVED_WIFI] = {"", ""};
int savedWiFiCount = 0;
bool portalClosed = false;
int currentWiFiSlot = 1;  // Track which WiFi we're on (1 or 2)
// Add this global flag at the top of your code
bool shouldStartServer = false;

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
  Serial.println("[DHT API URL] " + url);
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
  Serial.println("[Flow API URL] " + url);
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
  Serial.println("[SMIT API URL] " + url);
  http.begin(client, url);
  int httpCode = http.GET();
  Serial.println("[SMIT API] Code: " + String(httpCode));
  http.end();
}

// ─────────────────────────────────────────────────────
// Load Saved WiFi Networks from WiFiManager
// ─────────────────────────────────────────────────────
void loadSavedWiFiNetworks() {
  prefs.begin("wifi", true);
  savedSSIDs[0] = prefs.getString("ssid1", "");
  savedPasswords[0] = prefs.getString("pass1", "");
  savedSSIDs[1] = prefs.getString("ssid2", "");
  savedPasswords[1] = prefs.getString("pass2", "");
  prefs.end();
  
  savedWiFiCount = 0;
  if (savedSSIDs[0].length() > 0) savedWiFiCount++;
  if (savedSSIDs[1].length() > 0) savedWiFiCount++;
  
  Serial.println("[Prefs] WiFi 1: " + savedSSIDs[0]);
  Serial.println("[Prefs] WiFi 2: " + savedSSIDs[1]);
  Serial.println("[Prefs] Total Networks: " + String(savedWiFiCount));
}

// ─────────────────────────────────────────────────────
// Check if Portal Should Close
// ─────────────────────────────────────────────────────
void checkAndClosePortal() {
  if (savedWiFiCount >= 2 && !portalClosed) {
    Serial.println("[Portal] Closing WiFiManager portal - 2 networks saved");
    wm.stopWebPortal();
    portalClosed = true;
  }
}

void saveWiFiConfig() {
  loadSavedWiFiNetworks();  // ← Add this line at the top
  
  prefs.begin("wifi", false);
  if (savedSSIDs[0] == "") {
    prefs.putString("ssid1", WiFi.SSID());
    prefs.putString("pass1", WiFi.psk());
    Serial.println("[Prefs] Saved WiFi 1: " + WiFi.SSID());
  } else if (WiFi.SSID() != savedSSIDs[0] && savedSSIDs[1] == "") {
    prefs.putString("ssid2", WiFi.SSID());
    prefs.putString("pass2", WiFi.psk());
    Serial.println("[Prefs] Saved WiFi 2: " + WiFi.SSID());
  }
  prefs.end();
}

void saveConfigCallback() {
  Serial.println("[WM Callback] New WiFi saved via portal");
  delay(1000);
  loadSavedWiFiNetworks();
  saveWiFiConfig();
  loadSavedWiFiNetworks();
  
  Serial.println("[WM Callback] Total networks now: " + String(savedWiFiCount));
  
  if (savedWiFiCount >= 2) {
    Serial.println("[WM Callback] 2 WiFis saved - closing portal");
    wm.stopWebPortal();
    portalClosed = true;
    shouldStartServer = true; 
    // Start custom web server
    server.on("/", handleRoot);
    server.on("/reset", handleReset);
    server.begin();
    Serial.println("[WM Callback] Custom server started");
  }
} 

// ─────────────────────────────────────────────────────
// Web Server Handlers
// ─────────────────────────────────────────────────────
void handleRoot() {
  g_currentSSID = WiFi.SSID();
  String localIP = WiFi.localIP().toString();
  String html = "";

  bool onWiFi1 = (g_currentSSID == savedSSIDs[0]);
  String otherSSID = onWiFi1 ? savedSSIDs[1] : savedSSIDs[0];
  String bgColor = onWiFi1 ? "#e3f2fd" : "#f0f0f0";
  String title = onWiFi1 ? "WiFi 1 Connected" : "WiFi 2 Connected";

  Serial.println("[handleRoot] Current: " + g_currentSSID + " | Other: " + otherSSID);

  html = "<!DOCTYPE html><html><head><title>" + title + "</title>";
  html += "<style>body{font-family:Arial;text-align:center;padding:50px;background:" + bgColor + ";}";
  html += "button{padding:20px 40px;font-size:20px;background:#ff6b6b;color:white;border:none;border-radius:5px;cursor:pointer;}";
  html += "button:hover{background:#ff5252;}h1{color:#333;}</style></head><body>";
  html += "<h1>" + title + "</h1>";
  html += "<p>Current Network: <strong>" + g_currentSSID + "</strong></p>";
  html += "<p>Device IP: <strong>" + localIP + "</strong></p>";
  html += "<p>Saved WiFi 1: <strong>" + savedSSIDs[0] + "</strong></p>";
  html += "<p>Saved WiFi 2: <strong>" + savedSSIDs[1] + "</strong></p><br>";
  html += "<p>Switch to: <strong>" + otherSSID + "</strong></p><br>";
  html += "<button onclick=\"window.location.href='/reset'\">Reset</button>";
  html += "</body></html>";

  server.send(200, "text/html", html);
}

void handleReset() {
  String html = "<!DOCTYPE html><html><head><title>Switching WiFi...</title>";
  html += "<meta http-equiv='refresh' content='5;url=/'>";  // ← auto refresh after 5s
  html += "<style>body{font-family:Arial;text-align:center;padding:50px;}h1{color:#333;}</style>";
  html += "</head><body><h1>Switching WiFi...</h1>";
  html += "<p>Please wait, reconnecting...</p></body></html>";
  
  server.send(200, "text/html", html);
  delay(1000);
  
  String targetSSID, targetPassword;
  
  if (WiFi.SSID() == savedSSIDs[0]) {
    targetSSID = savedSSIDs[1];
    targetPassword = savedPasswords[1];
    currentWiFiSlot = 2;
  } else {
    targetSSID = savedSSIDs[0];
    targetPassword = savedPasswords[0];
    currentWiFiSlot = 1;
  }
  
  Serial.println("[Reset] Switching to: " + targetSSID);
  WiFi.disconnect(false);
  delay(500);
  WiFi.begin(targetSSID.c_str(), targetPassword.c_str());
  Serial.println("[Reset] WiFi.begin() called");
}

// ─────────────────────────────────────────────────────
// Setup
// ─────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\nStarting WiFi Setup Portal...");

  // Uncomment ONCE ONLY if you want to erase saved credentials
   //wm.resetSettings();
   //prefs.begin("wifi", false);
   //prefs.clear();
   //prefs.end();

  // ← MUST be before autoConnect
  wm.setSaveConfigCallback(saveConfigCallback);

  wm.setConfigPortalBlocking(true);
  wm.setConfigPortalTimeout(300);
  bool res = wm.autoConnect("ESP32_Setup", "12345678");

  if (res) {
    Serial.println("\nWiFi Connected!");
    Serial.println("SSID: " + WiFi.SSID());
    Serial.println("IP: " + WiFi.localIP().toString());

    // ← FIRST load saved networks before using savedWiFiCount
    loadSavedWiFiNetworks();

    // ← Save current WiFi if not already saved
    saveWiFiConfig();

    // ← Reload to get updated count
    loadSavedWiFiNetworks();

    // ← NOW set SSID tracking using loaded data
    g_currentSSID = WiFi.SSID();
    if (savedWiFiCount >= 2) {
      g_previousSSID = savedSSIDs[0];  // Force WiFi 1 as previous
    } else {
      g_previousSSID = g_currentSSID;
    }
    g_isFirstConnection = false;

    // ← Check if 2 WiFis are already saved
    if (savedWiFiCount >= 2) {
      Serial.println("[Setup] 2 WiFis saved - Starting custom server (Reset button)");
      server.on("/", handleRoot);
      server.on("/reset", handleReset);
      server.begin();
      Serial.println("[Web Server] Started on port 80 - Reset button mode");
      portalClosed = true;
    } else {
      Serial.println("[Setup] Less than 2 WiFis saved - Starting WiFiManager portal");
      wm.setConfigPortalBlocking(false);
      wm.startWebPortal();
    }

  } else {
    Serial.println("WiFi Connection Failed");
  }

  // ── Initialize LoRa ──────────────────────────
  Serial.println("\nInitializing LoRa...");

  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

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
  server.handleClient();

  if (shouldStartServer) {
    shouldStartServer = false;
    Serial.println("[Loop] Starting custom server now");
    server.stop();
    delay(500);
    server.on("/", handleRoot);
    server.on("/reset", handleReset);
    server.begin();
    Serial.println("[Loop] Custom server started successfully");
  }


  if (WiFi.status() == WL_CONNECTED && WiFi.SSID() != g_currentSSID) {
    Serial.println("[Loop] WiFi changed: " + g_currentSSID + " → " + WiFi.SSID());
    g_previousSSID = g_currentSSID;
    g_currentSSID = WiFi.SSID();
  }
  
// Reload saved WiFi networks periodically
static unsigned long lastCheck = 0;
static bool serverStarted = false;
static unsigned long lastIPPrint = 0;

if (millis() - lastCheck > 10000) {
  checkAndClosePortal();
  
  // Start custom server if portal just closed and 2 WiFis saved
  // Start custom server if portal just closed and 2 WiFis saved
if (portalClosed && !serverStarted && savedWiFiCount >= 2) {
  Serial.println("[Loop] Starting custom server now - 2 WiFis detected");
  server.stop();  // Stop any existing server first
  delay(100);
  server.on("/", handleRoot);
  server.on("/reset", handleReset);
  server.begin();
  Serial.println("[Loop] Custom server started successfully");
  serverStarted = true;
}
  
  lastCheck = millis();
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
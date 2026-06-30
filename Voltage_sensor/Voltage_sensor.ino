#include <WiFiManager.h>    
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <cmath>

const int sensorPin = 26;
const int numSamples = 1000;
#define voltage_API  "http://sunfra.com/farm/sensor/voltage_for_interns.php"

String g_voltage_value = "--"; 
String g_mac_address = "";
float sensitivity = 0.5604;

WiFiManager wm;
unsigned long lastAPICall = 0;
const unsigned long API_INTERVAL = 5000;  // 5 seconds between API calls

void sendvoltageAPI() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[API] WiFi not connected, skipping API call");
    return;
  }
  
  WiFiClient client;
  HTTPClient http;
  
  String url = String(voltage_API) + "?Voltage_Value=" + g_voltage_value + "&macID=" + g_mac_address;
  
  Serial.println("[API URL] " + url);
  http.begin(client, url);
  http.setTimeout(5000);  // 5 second timeout
  int httpCode = http.GET();
  Serial.println("[API Response] Code: " + String(httpCode));
  http.end();
}

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("\n\n");
    Serial.println("==================================");
    Serial.println(" ZMPT101B AC Voltage Reader");
    Serial.println("==================================");
    Serial.println("\nStarting WiFi Setup...");

    // WiFiManager Configuration
    wm.setConfigPortalBlocking(false);      // Non-blocking
    wm.setConfigPortalTimeout(180);         // 3 minutes timeout
    wm.setConnectTimeout(20);               // 20 seconds to connect
    wm.setWiFiAutoReconnect(true);          // Auto-reconnect enabled
    // REMOVED: wm.setPersistent(true);     // ← This method doesn't exist

    bool res = wm.autoConnect("VOLTAGE_SENSOR", "12345678");

    if (res) {
      Serial.println("\n✓ WiFi Connected!");
      Serial.println("SSID: " + WiFi.SSID());
      Serial.println("IP: " + WiFi.localIP().toString());
      Serial.println("Signal Strength: " + String(WiFi.RSSI()) + " dBm");
      
      // Get MAC address AFTER WiFi connects
      g_mac_address = WiFi.macAddress();
      Serial.println("MAC Address: " + g_mac_address);
      
      wm.startWebPortal();  // Start config portal in background
    } else {
      Serial.println("\n✗ WiFi Connection Failed");
      Serial.println("Entering AP mode. Connect to 'VOLTAGE_SENSOR' to configure.");
    }

    delay(1000);
}

float readVoltageRMS() {
    float sum = 0.0;
    float sumSquares = 0.0;
    int firstSample = 0;

    for (int i = 0; i < numSamples; i++) {
        int sample = analogRead(sensorPin);
        if (i == 0) firstSample = sample;
        sum += sample;
        sumSquares += (float)sample * sample;
        delayMicroseconds(100);
    }

    float midpoint = sum / numSamples;
    float meanSquare = (sumSquares / numSamples) - (midpoint * midpoint);
    if (meanSquare < 0) meanSquare = 0;
    float rmsRaw = sqrt(meanSquare);
    float voltage = rmsRaw * sensitivity;

    Serial.println("----------------------------------");
    Serial.print("Voltage: ");
    Serial.print(voltage, 2);
    Serial.println(" V");

    return voltage;
}

void loop() {
  // Keep WiFiManager running
  wm.process();
  
  // Check WiFi connection status
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("[WiFi] Connection Lost - Attempting to reconnect...");
    WiFi.reconnect();  // Try to reconnect
    delay(5000);       // Wait before checking again
    return;
  }

  // Only send API data every 5 seconds
  if (millis() - lastAPICall >= API_INTERVAL) {
    lastAPICall = millis();
    
    Serial.println("\n[Loop] Sending Data To APIs...");
    g_voltage_value = String(readVoltageRMS(), 2);
    sendvoltageAPI();
    Serial.println("[Loop] API Upload Complete\n");
  }
  
  delay(100);  // Small delay to prevent overwhelming the loop
}
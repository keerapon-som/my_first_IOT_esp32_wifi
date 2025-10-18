#include <Arduino.h>
#include <WebServer.h>
#include <ESPmDNS.h> // mDNS for http://<name>.local
#include "net/WiFiManager.h"
#include "api/Led.h"
#include "config.h"

const int HeartbeatIntervalMs = 20000;

// ===================== WiFi candidates =====================
const WifiCandidate CANDIDATES[] = {
    {WIFI_SSID_PRIMARY, WIFI_PASS_PRIMARY},
    {WIFI_SSID_BACKUP, WIFI_PASS_BACKUP}};

WebServer server(80);

// ===================== App State =====================
unsigned long lastHeartbeat = 0;

// ===================== WiFi Manager =====================
WiFiManager wifiManager(
    CANDIDATES,
    sizeof(CANDIDATES) / sizeof(CANDIDATES[0]), // <-- number of candidates
    WIFI_CHECK_INTERVAL                         // <-- periodic check interval (ms)
);

Led led(LED_PIN);

// ---------- HTTP Handlers ----------
static inline void markHttp() { wifiManager.markHttpActivity(); }

void handleRoot()
{
  markHttp();
  String page;
  page = F("<!DOCTYPE html><html><head><meta name='viewport' content='width=device-width, initial-scale=1'>");
  page += F("<title>ESP32-C3</title></head><body style='font-family:sans-serif;margin:1.5rem'>");
  page += F("<h1>ESP32-C3</h1>");
  page += F("<p>WIFI IP: <code>");
  page += wifiManager.ip().toString();
  page += F("</code></p>");
  page += F("<p>mDNS: <code>http://");
  page += MDNS_NAME;
  page += F(".local</code></p>");
  page += F("<p>SSID: <code>");
  page += wifiManager.currentSsid();
  page += F("</code></p>");
  page += F("<p>RSSI: <code>");
  page += String(wifiManager.currentRssi());
  page += F(" dBm</code></p>");
  page += F("<p>LED status: <strong>");
  page += (led.isOn() ? "ON" : "OFF");
  page += F("</strong></p>");
  page += F("<p><a href='/toggleLED'>Toggle LED</a></p>");
  page += F("</body></html>");
  server.send(200, "text/html", page);
}

void getWifiIP()
{
  markHttp();
  String page;
  page = F("<!DOCTYPE html><html><body>");
  page += F("<h1>ESP32-C3</h1>");
  page += F("<p>WIFI IP: ");
  page += wifiManager.ip().toString();
  page += F("</p>");
  page += F("</body></html>");
  server.send(200, "text/html", page);
}

void toggleLED()
{
  markHttp();
  server.send(200, "text/plain", led.toggle() ? "LED is ON" : "LED is OFF");
}

void handleNotFound()
{
  markHttp();
  server.send(404, "text/plain", "Not found: " + server.uri());
}

// ===================== Setup / Loop =====================
void setup()
{
  Serial.begin(115200);
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 1500)
  { /* brief wait for native USB boards */
  }

  // Bring up Wi-Fi (tries first candidate; the manager will optimize later)
  bool wifiOk = wifiManager.begin();

  // mDNS (optional)
  if (wifiOk)
  {
    if (MDNS.begin(MDNS_NAME))
    {
      MDNS.addService("http", "tcp", 80);
      Serial.print("mDNS started: http://");
      Serial.print(MDNS_NAME);
      Serial.println(".local");
    }
    else
    {
      Serial.println("mDNS start failed");
    }
  }

  // Routes
  server.on("/", HTTP_GET, handleRoot);
  server.on("/ip", HTTP_GET, getWifiIP);
  server.on("/toggleLED", HTTP_GET, toggleLED);
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP server started");
}

void loop()
{
  server.handleClient();

  // Heartbeat every 15s
  if (millis() - lastHeartbeat > HeartbeatIntervalMs)
  {
    lastHeartbeat = millis();
    if (wifiManager.isConnected())
    {
      Serial.print("IP: ");
      Serial.print(wifiManager.ip());
      Serial.print("  SSID: ");
      Serial.print(wifiManager.currentSsid());
      Serial.print("  RSSI: ");
      Serial.print(wifiManager.currentRssi());
      Serial.println(" dBm");
    }
    else
    {
      Serial.println("WiFi not connected");
    }
  }

  // Periodic scan & maybe switch (internals and throttling are handled inside)
  wifiManager.checkAndMaybeSwitchWiFi();

  // Note: No MDNS.update() on ESP32 — handled by the stack
}

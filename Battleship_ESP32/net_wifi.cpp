#include "net_wifi.h"
#include "game_config.h"
#include <WiFi.h>
#include <esp_mac.h>

static const char* s_ssid = nullptr;
static const char* s_pass = nullptr;
static char        s_mac[18] = {0};
static uint32_t    s_last_attempt_ms = 0;
static bool        s_was_connected = false;

void net_wifi_begin(const char* ssid, const char* pass) {
  s_ssid = ssid;
  s_pass = pass;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);   // modem sleep adds latency to MQTT

  // Read the MAC from eFuses rather than WiFi.macAddress(), which returns
  // all-zeros before the WiFi driver is up on some core versions.
  uint8_t mac[6] = {0};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(s_mac, sizeof(s_mac), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  Serial.printf("[wifi] MAC=%s, connecting to \"%s\"\n", s_mac, ssid);
  WiFi.begin(ssid, pass);
  s_last_attempt_ms = millis();
}

void net_wifi_loop() {
  bool connected = (WiFi.status() == WL_CONNECTED);

  if (connected != s_was_connected) {
    s_was_connected = connected;
    if (connected) {
      Serial.printf("[wifi] connected, IP=%s, RSSI=%d\n",
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
      Serial.println("[wifi] disconnected");
    }
  }

  if (!connected && millis() - s_last_attempt_ms > WIFI_RETRY_MS) {
    Serial.println("[wifi] reconnecting...");
    WiFi.disconnect();
    WiFi.begin(s_ssid, s_pass);
    s_last_attempt_ms = millis();
  }
}

bool net_wifi_is_connected() {
  return WiFi.status() == WL_CONNECTED;
}

const char* net_wifi_mac() {
  return s_mac;
}

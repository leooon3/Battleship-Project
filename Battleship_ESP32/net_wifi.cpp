// ============================================================================
// net_wifi.cpp
// ============================================================================
#include "net_wifi.h"
#include "game_config.h"
#include <WiFi.h>
#include <esp_mac.h>   // esp_read_mac() per leggere il MAC dagli eFuses

static const char* s_ssid = nullptr;
static const char* s_pass = nullptr;
static char s_mac[18] = {0};
static uint32_t s_last_attempt_ms = 0;
static bool s_was_connected = false;

void net_wifi_begin(const char* ssid, const char* pass) {
  s_ssid = ssid;
  s_pass = pass;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);   // disabilita sleep modem: piu' reattivo con MQTT

  // Per il test sulla rete TIM di casa usiamo DHCP (il router assegna IP).
  // Quando torneremo all'AP del Pi (che ha il dnsmasq rotto), riattivare le
  // 4 righe sotto per forzare IP statico nel range 192.168.4.0/24.
  // IPAddress local_ip(192, 168, 4, 150);
  // IPAddress gateway (192, 168, 4, 1);
  // IPAddress subnet  (255, 255, 255, 0);
  // IPAddress dns     (192, 168, 4, 1);
  // WiFi.config(local_ip, gateway, subnet, dns);

  // Memorizza il MAC nel formato canonico "AA:BB:CC:DD:EE:FF" (uppercase).
  // NOTA: usiamo esp_read_mac() invece di WiFi.macAddress() perche' su
  // alcune board/versioni del core arduino-esp32, WiFi.macAddress() chiamata
  // prima di WiFi.begin() restituisce 00:00:00:00:00:00 (driver WiFi non
  // ancora pronto). esp_read_mac() legge direttamente dagli eFuses, sempre
  // affidabile, e ci da' il vero STA MAC del chip.
  uint8_t mac_bytes[6] = {0};
  esp_read_mac(mac_bytes, ESP_MAC_WIFI_STA);
  snprintf(s_mac, sizeof(s_mac), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac_bytes[0], mac_bytes[1], mac_bytes[2],
           mac_bytes[3], mac_bytes[4], mac_bytes[5]);

  Serial.printf("[wifi] MAC=%s, connecting to SSID=\"%s\"\n", s_mac, ssid);
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

  if (!connected && (millis() - s_last_attempt_ms) > WIFI_RETRY_MS) {
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

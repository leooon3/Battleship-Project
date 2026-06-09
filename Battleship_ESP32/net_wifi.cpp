// ============================================================================
// net_wifi.cpp
// ============================================================================
#include "net_wifi.h"
#include "game_config.h"
#include <WiFi.h>

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

  // Workaround DHCP del Pi: il dnsmasq dell'AP NetworkManager attualmente non
  // assegna un IP. Usiamo IP statico nel range del Pi (192.168.4.0/24).
  // .100, .101, ... per le diverse unita': differenziabili modificando il
  // quarto ottetto qui sotto. Quando il DHCP del Pi sara' funzionante,
  // commentare le 4 righe WiFi.config(...) per tornare a DHCP.
  IPAddress local_ip(192, 168, 4, 150);
  IPAddress gateway (192, 168, 4, 1);
  IPAddress subnet  (255, 255, 255, 0);
  IPAddress dns     (192, 168, 4, 1);
  WiFi.config(local_ip, gateway, subnet, dns);

  // Memorizza il MAC nel formato canonico "AA:BB:CC:DD:EE:FF" (uppercase).
  // WiFi.macAddress() lo restituisce gia' cosi'.
  String mac = WiFi.macAddress();
  strncpy(s_mac, mac.c_str(), sizeof(s_mac) - 1);
  s_mac[sizeof(s_mac) - 1] = 0;

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

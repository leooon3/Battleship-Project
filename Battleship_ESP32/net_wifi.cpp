#include "net_wifi.h"
#include <WiFi.h>
#include <esp_mac.h>

static const WifiNet* s_nets = nullptr;
static size_t         s_count = 0;
static int            s_current = 0;
static char           s_mac[18] = {0};
static uint32_t       s_attempt_ms = 0;
static bool           s_was_connected = false;

// How long to wait for one network before trying the next.
static constexpr uint32_t PROFILE_TIMEOUT_MS = 8000;

static void connect_profile(int i) {
  const WifiNet& n = s_nets[i];
  s_current = i;

  if (n.static_ip && n.static_ip[0]) {
    IPAddress ip, gw, mask(255, 255, 255, 0);
    ip.fromString(n.static_ip);
    gw.fromString(n.gateway);
    WiFi.config(ip, gw, mask, gw);  // dns = gateway
  } else {
    WiFi.config(IPAddress((uint32_t)0), IPAddress((uint32_t)0), IPAddress((uint32_t)0));  // DHCP
  }

  WiFi.disconnect();
  WiFi.begin(n.ssid, n.pass);
  s_attempt_ms = millis();
  Serial.printf("[wifi] trying \"%s\"...\n", n.ssid);
}

// Highest-priority profile whose SSID is currently on air, or -1 if none.
static int scan_for_best() {
  int count = WiFi.scanNetworks();
  int best = -1;
  for (size_t p = 0; p < s_count && best < 0; p++) {
    for (int i = 0; i < count; i++) {
      if (WiFi.SSID(i) == s_nets[p].ssid) { best = p; break; }
    }
  }
  WiFi.scanDelete();
  return best;
}

void net_wifi_begin(const WifiNet* nets, size_t count) {
  s_nets = nets;
  s_count = count;

  WiFi.mode(WIFI_STA);
  WiFi.setAutoReconnect(true);
  WiFi.setSleep(false);

  // Read the MAC from eFuses: WiFi.macAddress() can be all-zeros before the
  // driver is up on some core versions.
  uint8_t mac[6] = {0};
  esp_read_mac(mac, ESP_MAC_WIFI_STA);
  snprintf(s_mac, sizeof(s_mac), "%02X:%02X:%02X:%02X:%02X:%02X",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.printf("[wifi] MAC=%s\n", s_mac);

  int best = scan_for_best();
  connect_profile(best >= 0 ? best : 0);
}

void net_wifi_loop() {
  bool connected = (WiFi.status() == WL_CONNECTED);

  if (connected != s_was_connected) {
    s_was_connected = connected;
    if (connected) {
      Serial.printf("[wifi] connected to \"%s\", IP=%s, RSSI=%d\n",
                    s_nets[s_current].ssid,
                    WiFi.localIP().toString().c_str(), WiFi.RSSI());
    } else {
      Serial.println("[wifi] disconnected");
    }
  }
  if (connected) return;

  if (millis() - s_attempt_ms > PROFILE_TIMEOUT_MS) {
    // Current network didn't come up. Prefer the best one on air (which may be
    // the same one again = retry); otherwise round-robin to the next.
    int best = scan_for_best();
    connect_profile(best >= 0 ? best : (s_current + 1) % s_count);
  }
}

bool net_wifi_is_connected() {
  return WiFi.status() == WL_CONNECTED;
}

const char* net_wifi_mac() {
  return s_mac;
}

const char* net_wifi_mqtt_host() {
  return s_nets[s_current].mqtt_host;
}

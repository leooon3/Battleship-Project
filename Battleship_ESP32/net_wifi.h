// WiFi connection with a priority list of networks. On boot it scans and joins
// the highest-priority network that's on air, falling back to the next ones.
#pragma once

#include <Arduino.h>

struct WifiNet {
  const char* ssid;
  const char* pass;
  const char* mqtt_host;   // broker reachable on this network
  const char* static_ip;   // "" or nullptr => DHCP
  const char* gateway;     // only used when static_ip is set
};

void net_wifi_begin(const WifiNet* nets, size_t count);
void net_wifi_loop();
bool net_wifi_is_connected();

const char* net_wifi_mac();          // "AA:BB:CC:DD:EE:FF"
const char* net_wifi_mqtt_host();    // broker for the network we're on

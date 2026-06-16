// WiFi station connection with auto-reconnect.
#pragma once

#include <Arduino.h>

void net_wifi_begin(const char* ssid, const char* pass);
void net_wifi_loop();
bool net_wifi_is_connected();

// MAC as "AA:BB:CC:DD:EE:FF" (static buffer, valid for the whole run).
const char* net_wifi_mac();

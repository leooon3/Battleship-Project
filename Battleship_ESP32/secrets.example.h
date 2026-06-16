// Copy this file to secrets.h and fill in the real values.
// secrets.h is gitignored, so credentials never reach the repo.
#pragma once

#define WIFI_SSID  "your-wifi"
#define WIFI_PASS  "your-password"

#define MQTT_HOST  "192.168.1.10"   // broker IP (the machine running Mosquitto)
#define MQTT_PORT  1883

// Uncomment if the broker requires auth (the server runs anonymous by default).
// #define MQTT_USER  "..."
// #define MQTT_PASS  "..."

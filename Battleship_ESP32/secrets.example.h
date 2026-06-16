// Copy this file to secrets.h and fill in the real values.
// secrets.h is gitignored, so credentials never reach the repo.
//
// Two networks are tried in order: the firmware scans on boot and joins the
// first one that's on air. Put the Raspberry Pi AP first and a fallback (e.g.
// home WiFi) second. Each network has its own broker address because they
// usually differ (the broker may run on the Pi, or on a dev PC).
#pragma once

// Primary: Raspberry Pi access point.
#define WIFI_PI_SSID  "raspberry-ap"
#define WIFI_PI_PASS  "your-password"
#define WIFI_PI_MQTT  "192.168.4.1"    // broker on this network
#define WIFI_PI_IP    ""               // "" = DHCP; or a static IP like "192.168.4.150"
#define WIFI_PI_GW    "192.168.4.1"    // gateway, only used when WIFI_PI_IP is set

// Fallback: home / dev WiFi.
#define WIFI_FB_SSID  "your-wifi"
#define WIFI_FB_PASS  "your-password"
#define WIFI_FB_MQTT  "192.168.1.10"   // broker on this network (e.g. the dev PC)
#define WIFI_FB_IP    ""               // DHCP
#define WIFI_FB_GW    ""

#define MQTT_PORT 1883

// Uncomment if the broker requires auth.
// #define MQTT_USER  "..."
// #define MQTT_PASS  "..."

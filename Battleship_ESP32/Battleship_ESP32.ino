// Battleship ESP32 firmware - the "bridge" half: WiFi, MQTT, the Rust server
// protocol, the match state machine and the local game state. The joystick and
// LED matrix are driven by the other team in this same sketch (see hal.h).
//
// Setup:
//   1. Libraries (Library Manager): espMqttClient (Bert Melis),
//      AsyncTCP (ESP32Async), ArduinoJson v7
//   2. Copy secrets.example.h to secrets.h and fill it in
//   3. Board: ESP32 WROOM-DA Module (or equivalent), then Upload
//   4. Serial Monitor at 115200 baud

#include "secrets.h"
#include "net_wifi.h"
#include "net_mqtt.h"
#include "app_fsm.h"

// Networks to try, highest priority first: the Raspberry Pi AP if it's on air,
// otherwise the fallback (home WiFi with the dev broker on the PC). Each entry
// carries its own broker address since they differ per network.
static const WifiNet NETS[] = {
  { WIFI_PI_SSID, WIFI_PI_PASS, WIFI_PI_MQTT, WIFI_PI_IP, WIFI_PI_GW },
  { WIFI_FB_SSID, WIFI_FB_PASS, WIFI_FB_MQTT, WIFI_FB_IP, WIFI_FB_GW },
};

static void start_mqtt(const char* host) {
#ifdef MQTT_USER
  net_mqtt_begin(host, MQTT_PORT, MQTT_USER, MQTT_PASS, net_wifi_mac());
#else
  net_mqtt_begin(host, MQTT_PORT, nullptr, nullptr, net_wifi_mac());
#endif
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Battleship ESP32 (bridge) ===");

  net_wifi_begin(NETS, sizeof(NETS) / sizeof(NETS[0]));
  app_fsm_begin();
}

void loop() {
  net_wifi_loop();

  // Bring MQTT up once WiFi is connected, and re-point it at the right broker
  // if we ever fall back to a different network.
  static const char* mqtt_host = nullptr;
  if (net_wifi_is_connected()) {
    const char* host = net_wifi_mqtt_host();
    if (host != mqtt_host) {   // static strings: pointer compare is enough
      mqtt_host = host;
      start_mqtt(host);
    }
    net_mqtt_loop();
  }

  app_fsm_loop();
  delay(2);
}

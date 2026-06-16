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

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== Battleship ESP32 (bridge) ===");

  net_wifi_begin(WIFI_SSID, WIFI_PASS);

  // Give WiFi a head start before MQTT; it retries internally if not ready.
  uint32_t t0 = millis();
  while (!net_wifi_is_connected() && millis() - t0 < 15000) {
    net_wifi_loop();
    delay(100);
  }

#ifdef MQTT_USER
  net_mqtt_begin(MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASS, net_wifi_mac());
#else
  net_mqtt_begin(MQTT_HOST, MQTT_PORT, nullptr, nullptr, net_wifi_mac());
#endif

  app_fsm_begin();
}

void loop() {
  net_wifi_loop();
  net_mqtt_loop();
  app_fsm_loop();
  delay(2);
}

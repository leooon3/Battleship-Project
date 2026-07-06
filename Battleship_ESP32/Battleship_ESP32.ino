// Minimal test: the ESP32 connects to WiFi + MQTT and registers with the
// server, then prints whether it was assigned host or guest. No input, no
// display — just the connection + register handshake.
//
// The full facade (battle.h) has more (setup, shoot, ...); this driver only
// uses what's needed for this test.
//
// Libraries: espMqttClient (Bert Melis), AsyncTCP (ESP32Async), ArduinoJson v7.

#include "battle.h"

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== Battleship connection test ===");

  battle_begin();                  // WiFi + MQTT (blocks until connected)
  Serial.println("MQTT connected.");

  Role me = battle_register();     // register + wait for the assigned role
  Serial.printf("registered as: %s\n", role_to_str(me));
}

void loop() {
  delay(1000);
}

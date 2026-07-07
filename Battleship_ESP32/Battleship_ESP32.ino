// Entry point for the game.
//
// Libraries: espMqttClient (Bert Melis), AsyncTCP (ESP32Async), ArduinoJson v7,
// Adafruit GFX / NeoMatrix / NeoPixel. Fill in secrets.h, flash at 115200.

#include "interface.h"

void setup() {
  interface_setup();
}

void loop() {
  interface_loop();
}

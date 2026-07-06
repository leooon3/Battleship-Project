// TEST driver for the synchronous facade (battle.h). Plays a full game in a
// linear, blocking style, reading shots from the Serial Monitor, so the facade
// can be exercised without joystick/matrix hardware.
//
// In production this file is replaced by the interface team's sketch: their
// display + joystick code, calling the same battle_* functions in the same
// linear way.
//
// Libraries: espMqttClient (Bert Melis), AsyncTCP (ESP32Async), ArduinoJson v7.
// Fill in secrets.h, flash, open Serial Monitor at 115200.

#include "interface.h"

void setup() {
  interface_setup();
}

void loop() {
  interface_loop();
}

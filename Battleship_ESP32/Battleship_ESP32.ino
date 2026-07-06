// Entry point: runs the interface team's code (matrix + joystick), which drives
// the facade (battle.*). This is the real game with hardware.
//
// The Serial-only facade test driver (no matrix/joystick needed) lives in git
// history, in the commit before the interface integration — restore it there if
// you ever need to test the facade without the hardware.
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

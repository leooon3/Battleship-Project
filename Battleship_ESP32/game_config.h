// Game and network tunables. Display/joystick settings live in the other
// team's files, not here.
#pragma once

#include <Arduino.h>

// Board size, matching the Rust server's 8x8 default.
constexpr int BOARD_W = 8;
constexpr int BOARD_H = 8;

// Fleet to place: one 2-cell ship plus three 1-cell ships. The setup wizard
// offers them in this order (longest first).
constexpr int FLEET_LENS[] = { 2, 1, 1, 1 };
constexpr int FLEET_COUNT  = sizeof(FLEET_LENS) / sizeof(FLEET_LENS[0]);

constexpr uint32_t MQTT_RETRY_MS    = 2000;
constexpr uint16_t MQTT_KEEPALIVE_S = 15;

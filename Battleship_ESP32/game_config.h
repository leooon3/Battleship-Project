// Game and network tunables
#pragma once

#include <Arduino.h>

// Board size, matching the server's one
constexpr int BOARD_W = 8;
constexpr int BOARD_H = 8;

// Fleet to place: two 2-cell ships, one 3-cell, one 4-cell
constexpr int FLEET_LENS[] = { 4, 3, 2, 2 };
constexpr int FLEET_COUNT  = sizeof(FLEET_LENS) / sizeof(FLEET_LENS[0]);

constexpr uint32_t MQTT_RETRY_MS    = 2000;
constexpr uint16_t MQTT_KEEPALIVE_S = 15;

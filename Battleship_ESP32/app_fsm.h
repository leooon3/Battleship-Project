// Game phase, updated as the match progresses
#pragma once

#include <Arduino.h>

enum class AppPhase : uint8_t {
  Init,
  WaitingNet,        // connecting to WiFi + MQTT
  Ready,             // connected, before the player starts
  Registering,       // register sent, waiting for the assigned role
  SettingUp,         // assigned, placing ships
  WaitingGameStart,  // setup sent, waiting for the game to start
  Playing,
  End,               // game over
};

AppPhase    app_fsm_phase();
const char* app_fsm_phase_str();

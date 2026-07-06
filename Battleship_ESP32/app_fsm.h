// Game phase, for driving the display. The facade (battle.cpp) keeps this
// updated as the match progresses. Read it if you prefer switching on a phase
// to decide what to draw, instead of following the linear battle_* flow.
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

// Main state machine. Drives MQTT (register/assign/setup/shoot) from joystick
// input and incoming server messages. The display team reads the resulting
// phase via app_fsm_phase().
#pragma once

#include <Arduino.h>

enum class AppPhase : uint8_t {
  Init,
  WaitingNet,        // WiFi + MQTT connecting
  Ready,             // connected, idle screen, waiting for the start button
  Registering,       // register sent, waiting for assign
  SettingUp,         // placing ships
  WaitingGameStart,  // setup sent, waiting for the first state
  Playing,
  End,               // server declared a winner (see g_state.winner)
};

void        app_fsm_begin();
void        app_fsm_loop();
AppPhase    app_fsm_phase();
const char* app_fsm_phase_str();

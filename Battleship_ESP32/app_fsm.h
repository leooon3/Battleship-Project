// ============================================================================
// app_fsm.h
//
// Macchina a stati principale. Coordina rete (net_mqtt) + stato di gioco
// (game_state) + input dal modulo joystick (hal.h: app_on_input).
//
// Non sa nulla di display: il renderer legge dallo stato pubblicato qui.
// ============================================================================
#pragma once

#include <Arduino.h>

enum class AppPhase : uint8_t {
  Init,
  WaitingNet,         // attesa WiFi + MQTT
  Registering,        // pubblicato register, attesa assign
  SettingUp,          // wizard interattivo di piazzamento navi
  WaitingGameStart,   // setup inviato, attesa primo state dal server
  Playing,            // partita in corso
  End,                // (al momento mai raggiunto: nessun evento di fine dal server)
};

void        app_fsm_begin();
void        app_fsm_loop();
AppPhase    app_fsm_phase();
const char* app_fsm_phase_str();

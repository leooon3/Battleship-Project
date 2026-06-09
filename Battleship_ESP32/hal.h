// ============================================================================
// hal.h
//
// Confine tra il nostro firmware (MQTT bridge + FSM + stato di gioco) e il
// modulo joystick scritto dall'altro team. Una sola cosa qui dentro:
//
//   IL TEAM JOYSTICK CHIAMA  app_on_input(InputEvent)  quando rileva un
//   movimento del joystick o un click del pulsante.
//
// La nostra FSM accoda l'evento e lo consuma nel loop. Niente lettura ADC,
// niente debounce, niente repeat-rate: quelle scelte le fa loro.
//
// Per quanto riguarda il DISPLAY: noi NON guidiamo i LED. Il team display
// legge lo stato di gioco direttamente dal singleton g_state (game_state.h)
// e disegna come preferisce.
// ============================================================================
#pragma once

#include <Arduino.h>

enum class InputEvent : uint8_t {
  Up,         // joystick verso l'alto  (y+1)
  Down,       // joystick verso il basso (y-1)
  Left,       // joystick a sinistra    (x-1)
  Right,      // joystick a destra      (x+1)
  BtnShort,   // click corto: ruota la nave durante setup / spara durante play
  BtnLong,    // click lungo: conferma piazzamento / passa alla prossima nave
};

// Implementata da NOI in app_fsm.cpp. Sicura da chiamare anche da ISR (le
// scrive su una coda lock-free, niente lavoro pesante nel chiamante).
void app_on_input(InputEvent e);

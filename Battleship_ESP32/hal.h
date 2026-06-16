// Boundary with the joystick/display team.
//
// They call app_on_input() when the stick moves or the button is clicked; we
// queue the event and act on it in the FSM. They read game state straight from
// g_state (game_state.h) to drive the LED matrix. We never touch GPIO/ADC/LEDs.
#pragma once

#include <Arduino.h>

enum class InputEvent : uint8_t {
  Up,
  Down,
  Left,
  Right,
  BtnShort,   // setup: rotate ship / play: fire
  BtnLong,    // setup: confirm placement
};

// Safe to call from an ISR: just pushes onto a lock-free queue.
void app_on_input(InputEvent e);

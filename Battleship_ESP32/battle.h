// Synchronous API for the game logic.
// WiFi + MQTT run on a dedicated core, so blocking calls don't kill the connection.
#pragma once

#include "protocol.h"
#include <Arduino.h>

void      battle_begin();                             // connect (blocks until MQTT is up)
Role      battle_register();                          // register + wait for the assigned role
void      battle_send_setup(const Boat* boats, size_t n);  // send fleet, returns immediately
bool      battle_game_started();                          // true once both players are ready
bool      battle_my_turn();                           // non-blocking
bool      battle_over();                              // non-blocking
Role      battle_winner();                            // valid once battle_over() is true
HitResult battle_shoot(uint8_t x, uint8_t y);         // fire + wait for the result
void      battle_await_change();                      // block until my turn again, or game over
HitResult battle_wait_for_opponent_shot(uint8_t& out_x, uint8_t& out_y); // block until opponent fires


// Synchronous facade for the interface team.
//
// Every battle_* call reads like a normal blocking function: it returns when the
// server round-trip it represents is done. Under the hood WiFi + MQTT run on a
// dedicated core, so the connection stays alive even while the caller (or the
// interface's own code) blocks.
//
// Typical linear use:
//
//   battle_begin();                       // connect
//   ... show waiting screen, wait button ...
//   Role me = battle_register();          // pair with the server
//   ... place ships, fill boats[] ...
//   battle_send_setup(boats, n);          // send + wait for the game to start
//   while (!battle_over()) {
//     if (battle_my_turn()) {
//       ... aim (x,y) ...
//       HitResult r = battle_shoot(x, y);
//       ... draw the shot result ...
//     } else {
//       uint8_t ox, oy;
//       HitResult r = battle_wait_for_opponent_shot(ox, oy);
//       ... draw the opponent's shot at (ox, oy) ...
//     }
//   }
//   ... show battle_winner() ...
//
// Boards to draw are in g_state (game_state.h), updated automatically.
#pragma once

#include "protocol.h"
#include <Arduino.h>

void      battle_begin();                             // connect (blocks until MQTT is up)
Role      battle_register();                          // register + wait for the assigned role
void      battle_send_setup(const Boat* boats, size_t n);  // send fleet + wait for game start
bool      battle_my_turn();                           // non-blocking
bool      battle_over();                              // non-blocking
Role      battle_winner();                            // valid once battle_over() is true
HitResult battle_shoot(uint8_t x, uint8_t y);         // fire + wait for the result
void      battle_await_change();                      // block until my turn again, or game over
HitResult battle_wait_for_opponent_shot(uint8_t& out_x, uint8_t& out_y); // block until opponent fires, return coords and hit result


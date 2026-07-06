// Read-only view of the boards, for the display. The facade (battle.cpp) writes
// it from incoming server events; the interface team reads it to draw. Control
// flow (turn, winner, ...) is exposed through battle.h, not here.
#pragma once

#include "game_config.h"
#include "protocol.h"
#include <Arduino.h>

// A cell of my own fleet.
enum class OwnCell : uint8_t {
  Empty,   // water
  Ship,    // intact ship
  Hit,     // ship cell hit
  Sunk,    // ship cell of a sunk ship
  Miss,    // opponent fired here and missed
};

// What I know about a cell of the opponent's board.
enum class EnemyCell : uint8_t {
  Unknown, // not fired at yet
  Miss,    // fired and missed
  Hit,     // fired and hit (ship still afloat)
  Sunk,    // fired and sank a ship
};

struct GameState {
  Role     my_role = Role::None;
  uint32_t game_id = 0;

  OwnCell   own_board  [BOARD_W][BOARD_H];
  EnemyCell enemy_board[BOARD_W][BOARD_H];

  uint8_t my_ships_sunk    = 0;  // progress info for the display
  uint8_t enemy_ships_sunk = 0;
};

extern GameState g_state;

void game_state_reset();

// Mark a placed ship on own_board (called for each ship before sending setup).
void game_state_mark_ship(const Boat& b);

// Apply a server event to own_board or enemy_board.
void game_state_apply_event(const proto::EventMsg& e);

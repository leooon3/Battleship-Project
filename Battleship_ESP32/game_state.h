// Read-only view of the boards, for the display. battle.cpp writes
// it from incoming server events; the interface reads it to draw.
#pragma once

#include "game_config.h"
#include "protocol.h"
#include <Arduino.h>

// A cell of my fleet.
enum class OwnCell : uint8_t {
  Empty,   // water
  Ship,    // intact ship
  Hit,     // hit => ship still afloat
  Sunk,    // sunk => ship sunk
  Miss,    // missed => water
};

// What I know about a cell of the opponent's board.
enum class EnemyCell : uint8_t {
  Unknown, // led off
  Miss,    // missed => water
  Hit,     // hit => ship still afloat
  Sunk,    // sank => ship sunk
};

struct GameState {
  Role     my_role = Role::None;
  uint32_t game_id = 0;

  OwnCell   own_board  [BOARD_W][BOARD_H];
  EnemyCell enemy_board[BOARD_W][BOARD_H];

  uint8_t my_ships_sunk    = 0;  // progress info for display
  uint8_t enemy_ships_sunk = 0;
};

extern GameState g_state;

void game_state_reset();

// Mark a placed ship on own_board
void game_state_mark_ship(const Boat& b);

// Apply a server event to own_board or enemy_board 
void game_state_apply_event(const proto::EventMsg& e);

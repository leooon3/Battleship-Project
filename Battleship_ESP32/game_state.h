// Local game state. Lives in the g_state singleton: the FSM writes it, the
// display team reads it. Knows nothing about MQTT or LEDs.
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
  Role     turn    = Role::None;

  OwnCell   own_board  [BOARD_W][BOARD_H];
  EnemyCell enemy_board[BOARD_W][BOARD_H];

  // Cursor, used both when placing ships and when aiming.
  uint8_t cursor_x = 0;
  uint8_t cursor_y = 0;

  // Setup wizard.
  uint8_t   setup_index = 0;            // next ship to place (index into FLEET_LENS)
  Direction setup_dir   = Direction::East;
  Boat      placed_boats[FLEET_COUNT];

  // Sunk ship counts, for display (progress). Win/loss comes from the server.
  uint8_t my_ships_sunk    = 0;
  uint8_t enemy_ships_sunk = 0;

  // Set when the server declares the game over (phase becomes End).
  Role winner = Role::None;
};

extern GameState g_state;

void game_state_reset();
void game_state_cursor_move(int8_t dx, int8_t dy);

// Setup wizard. current_len is 0 once every ship is placed.
uint8_t game_state_setup_current_len();
void    game_state_setup_rotate();
bool    game_state_setup_complete();
bool    game_state_setup_valid_placement(uint8_t x, uint8_t y, Direction dir, uint8_t len);

// Place the current ship at the cursor. Returns false (and does nothing) if the
// placement is invalid.
bool game_state_setup_try_confirm();

// Cells the in-progress ship would occupy, clipped to the grid. Writes into
// outx/outy (size them to the longest fleet ship) and returns the count.
uint8_t game_state_setup_preview(uint8_t outx[], uint8_t outy[]);

// Copy the placed ships out for sending. Returns how many were written.
size_t game_state_setup_get_boats(Boat* out, size_t out_max);

// Apply a server event to own_board or enemy_board.
void game_state_apply_event(const proto::EventMsg& e);

// True if the cell under the cursor on the enemy board hasn't been fired at.
bool game_state_aim_can_shoot();

#include "game_state.h"
#include <string.h>

GameState g_state;

void game_state_reset() {
  memset(&g_state, 0, sizeof(g_state));  // Empty / Unknown / 0 are all correct
  g_state.my_role = Role::None;
}

void game_state_mark_ship(const Boat& b) {
  int8_t dx = 0, dy = 0;
  switch (b.dir) {
    case Direction::North: dy = +1; break;
    case Direction::East:  dx = +1; break;
    case Direction::South: dy = -1; break;
    case Direction::West:  dx = -1; break;
  }
  for (uint8_t i = 0; i < b.len; i++) {
    int x = b.x + dx * i;
    int y = b.y + dy * i;
    if (x >= 0 && x < BOARD_W && y >= 0 && y < BOARD_H)
      g_state.own_board[x][y] = OwnCell::Ship;
  }
}

void game_state_apply_event(const proto::EventMsg& e) {
  if (e.x >= BOARD_W || e.y >= BOARD_H) return;

  if (e.attacker == g_state.my_role) {
    // My shot landed on the opponent's board.
    EnemyCell& c = g_state.enemy_board[e.x][e.y];
    switch (e.hit) {
      case HitResult::Water: c = EnemyCell::Miss; break;
      case HitResult::Hit:   c = EnemyCell::Hit;  break;
      case HitResult::Sunk:  c = EnemyCell::Sunk; g_state.enemy_ships_sunk++; break;
    }
  } else {
    // The opponent fired at my board.
    OwnCell& c = g_state.own_board[e.x][e.y];
    switch (e.hit) {
      case HitResult::Water: if (c == OwnCell::Empty) c = OwnCell::Miss; break;
      case HitResult::Hit:   c = OwnCell::Hit; break;
      case HitResult::Sunk:  c = OwnCell::Sunk; g_state.my_ships_sunk++; break;
    }
  }
}

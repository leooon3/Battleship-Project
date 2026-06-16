#include "game_state.h"
#include <string.h>

GameState g_state;

static void dir_to_delta(Direction d, int8_t& dx, int8_t& dy) {
  switch (d) {
    case Direction::North: dx =  0; dy = +1; break;
    case Direction::East:  dx = +1; dy =  0; break;
    case Direction::South: dx =  0; dy = -1; break;
    case Direction::West:  dx = -1; dy =  0; break;
  }
}

// Fill the cells a ship covers (may fall outside the grid; caller checks).
static void boat_cells(uint8_t x, uint8_t y, Direction dir, uint8_t len,
                       int xs[], int ys[]) {
  int8_t dx, dy;
  dir_to_delta(dir, dx, dy);
  for (uint8_t i = 0; i < len; i++) {
    xs[i] = x + dx * i;
    ys[i] = y + dy * i;
  }
}

static bool in_grid(int x, int y) {
  return x >= 0 && x < BOARD_W && y >= 0 && y < BOARD_H;
}

void game_state_reset() {
  memset(&g_state, 0, sizeof(g_state));  // Empty / Unknown are 0
  g_state.my_role   = Role::None;
  g_state.turn      = Role::None;
  g_state.cursor_x  = BOARD_W / 2;
  g_state.cursor_y  = BOARD_H / 2;
  g_state.setup_dir = Direction::East;
}

void game_state_cursor_move(int8_t dx, int8_t dy) {
  int nx = constrain(g_state.cursor_x + dx, 0, BOARD_W - 1);
  int ny = constrain(g_state.cursor_y + dy, 0, BOARD_H - 1);
  g_state.cursor_x = nx;
  g_state.cursor_y = ny;
}

uint8_t game_state_setup_current_len() {
  if (g_state.setup_index >= FLEET_COUNT) return 0;
  return FLEET_LENS[g_state.setup_index];
}

void game_state_setup_rotate() {
  switch (g_state.setup_dir) {
    case Direction::North: g_state.setup_dir = Direction::East;  break;
    case Direction::East:  g_state.setup_dir = Direction::South; break;
    case Direction::South: g_state.setup_dir = Direction::West;  break;
    case Direction::West:  g_state.setup_dir = Direction::North; break;
  }
}

bool game_state_setup_complete() {
  return g_state.setup_index >= FLEET_COUNT;
}

bool game_state_setup_valid_placement(uint8_t x, uint8_t y, Direction dir, uint8_t len) {
  if (len == 0 || len > BOARD_W) return false;  // can't fit a longer ship anyway
  int xs[BOARD_W], ys[BOARD_H];
  boat_cells(x, y, dir, len, xs, ys);

  for (uint8_t i = 0; i < len; i++) {
    if (!in_grid(xs[i], ys[i])) return false;
  }

  // No overlap with ships already placed.
  for (uint8_t bi = 0; bi < g_state.setup_index; bi++) {
    const Boat& b = g_state.placed_boats[bi];
    int bxs[BOARD_W], bys[BOARD_H];
    boat_cells(b.x, b.y, b.dir, b.len, bxs, bys);
    for (uint8_t i = 0; i < len; i++)
      for (uint8_t j = 0; j < b.len; j++)
        if (xs[i] == bxs[j] && ys[i] == bys[j]) return false;
  }
  return true;
}

bool game_state_setup_try_confirm() {
  uint8_t len = game_state_setup_current_len();
  if (len == 0) return false;
  if (!game_state_setup_valid_placement(g_state.cursor_x, g_state.cursor_y,
                                        g_state.setup_dir, len)) return false;

  Boat& b = g_state.placed_boats[g_state.setup_index];
  b = { g_state.cursor_x, g_state.cursor_y, g_state.setup_dir, len };

  int xs[BOARD_W], ys[BOARD_H];
  boat_cells(b.x, b.y, b.dir, b.len, xs, ys);
  for (uint8_t i = 0; i < b.len; i++)
    g_state.own_board[xs[i]][ys[i]] = OwnCell::Ship;

  g_state.setup_index++;
  return true;
}

uint8_t game_state_setup_preview(uint8_t outx[], uint8_t outy[]) {
  uint8_t len = game_state_setup_current_len();
  if (len == 0) return 0;
  int xs[BOARD_W], ys[BOARD_H];
  boat_cells(g_state.cursor_x, g_state.cursor_y, g_state.setup_dir, len, xs, ys);
  uint8_t n = 0;
  for (uint8_t i = 0; i < len; i++) {
    if (in_grid(xs[i], ys[i])) {
      outx[n] = xs[i];
      outy[n] = ys[i];
      n++;
    }
  }
  return n;
}

size_t game_state_setup_get_boats(Boat* out, size_t out_max) {
  size_t n = min((size_t)g_state.setup_index, out_max);
  for (size_t i = 0; i < n; i++) out[i] = g_state.placed_boats[i];
  return n;
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

bool game_state_aim_can_shoot() {
  return g_state.enemy_board[g_state.cursor_x][g_state.cursor_y] == EnemyCell::Unknown;
}

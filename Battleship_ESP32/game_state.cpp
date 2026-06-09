// ============================================================================
// game_state.cpp
// ============================================================================
#include "game_state.h"
#include <string.h>

GameState g_state;

// ---------------- Helpers locali ----------------

// Restituisce dx, dy per una direzione di una unita'.
static void dir_to_delta(Direction d, int8_t& dx, int8_t& dy) {
  switch (d) {
    case Direction::North: dx =  0; dy = +1; break;
    case Direction::East:  dx = +1; dy =  0; break;
    case Direction::South: dx =  0; dy = -1; break;
    case Direction::West:  dx = -1; dy =  0; break;
  }
}

// Calcola tutte le celle occupate dalla nave (x,y,dir,len). Le scrive in
// out[0..len-1]. Le celle possono essere fuori dalla griglia: il chiamante
// deve verificare se serve.
static void compute_boat_cells(uint8_t x, uint8_t y, Direction dir, uint8_t len,
                               int xs[], int ys[]) {
  int8_t dx, dy;
  dir_to_delta(dir, dx, dy);
  for (uint8_t i = 0; i < len; i++) {
    xs[i] = (int)x + (int)dx * (int)i;
    ys[i] = (int)y + (int)dy * (int)i;
  }
}

static bool in_grid(int x, int y) {
  return x >= 0 && x < BOARD_W && y >= 0 && y < BOARD_H;
}

// ---------------- API ----------------

void game_state_reset() {
  memset(&g_state, 0, sizeof(g_state));
  g_state.my_role  = Role::None;
  g_state.turn     = Role::None;
  g_state.cursor_x = BOARD_W / 2;
  g_state.cursor_y = BOARD_H / 2;
  g_state.setup_dir = Direction::East;
  // own_board/enemy_board sono gia' azzerati (Empty / Unknown corrispondono a 0)
}

// ---- Cursor ----

void game_state_cursor_move(int8_t dx, int8_t dy) {
  int nx = (int)g_state.cursor_x + dx;
  int ny = (int)g_state.cursor_y + dy;
  if (nx < 0) nx = 0;
  if (ny < 0) ny = 0;
  if (nx >= BOARD_W) nx = BOARD_W - 1;
  if (ny >= BOARD_H) ny = BOARD_H - 1;
  g_state.cursor_x = (uint8_t)nx;
  g_state.cursor_y = (uint8_t)ny;
}

// ---- Setup wizard ----

uint8_t game_state_setup_current_len() {
  if (g_state.setup_index >= FLEET_COUNT) return 0;
  return (uint8_t)FLEET_LENS[g_state.setup_index];
}

Direction game_state_setup_current_dir() {
  return g_state.setup_dir;
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

bool game_state_setup_valid_placement(uint8_t x, uint8_t y,
                                      Direction dir, uint8_t len) {
  int xs[16], ys[16];
  if (len > 16) return false;
  compute_boat_cells(x, y, dir, len, xs, ys);

  // 1) tutte le celle dentro griglia
  for (uint8_t i = 0; i < len; i++) {
    if (!in_grid(xs[i], ys[i])) return false;
  }

  // 2) niente sovrapposizioni con navi gia' piazzate
  for (uint8_t bi = 0; bi < g_state.setup_index; bi++) {
    const Boat& b = g_state.placed_boats[bi];
    int bxs[16], bys[16];
    if (b.len > 16) continue;
    compute_boat_cells(b.x, b.y, b.dir, b.len, bxs, bys);
    for (uint8_t i = 0; i < len; i++) {
      for (uint8_t j = 0; j < b.len; j++) {
        if (xs[i] == bxs[j] && ys[i] == bys[j]) return false;
      }
    }
  }

  return true;
}

bool game_state_setup_try_confirm() {
  uint8_t len = game_state_setup_current_len();
  if (len == 0) return false;

  if (!game_state_setup_valid_placement(g_state.cursor_x, g_state.cursor_y,
                                        g_state.setup_dir, len)) {
    return false;
  }

  // Aggiungi alla lista
  Boat& b = g_state.placed_boats[g_state.setup_index];
  b.x   = g_state.cursor_x;
  b.y   = g_state.cursor_y;
  b.dir = g_state.setup_dir;
  b.len = len;

  // Marca le celle nella own_board come Ship
  int xs[16], ys[16];
  compute_boat_cells(b.x, b.y, b.dir, b.len, xs, ys);
  for (uint8_t i = 0; i < b.len; i++) {
    g_state.own_board[xs[i]][ys[i]] = OwnCell::Ship;
  }

  g_state.setup_index++;
  // (la direzione di default per la prossima nave resta quella corrente)
  return true;
}

void game_state_setup_foreach_preview_cell(
    void (*cb)(uint8_t x, uint8_t y, void* user), void* user) {
  uint8_t len = game_state_setup_current_len();
  if (len == 0 || !cb) return;
  int xs[16], ys[16];
  compute_boat_cells(g_state.cursor_x, g_state.cursor_y,
                     g_state.setup_dir, len, xs, ys);
  for (uint8_t i = 0; i < len; i++) {
    if (in_grid(xs[i], ys[i])) {
      cb((uint8_t)xs[i], (uint8_t)ys[i], user);
    }
  }
}

size_t game_state_setup_get_boats(Boat* out, size_t out_max) {
  size_t n = (g_state.setup_index < out_max) ? g_state.setup_index : out_max;
  for (size_t i = 0; i < n; i++) {
    out[i] = g_state.placed_boats[i];
  }
  return n;
}

// ---- Eventi di gioco ----

void game_state_apply_event(const proto::EventMsg& e) {
  if (e.x >= BOARD_W || e.y >= BOARD_H) return;

  bool i_attacked = (e.attacker == g_state.my_role);

  if (i_attacked) {
    // Aggiorno la mia visione della board avversaria.
    EnemyCell& c = g_state.enemy_board[e.x][e.y];
    switch (e.hit) {
      case HitResult::Water: c = EnemyCell::Miss; break;
      case HitResult::Hit:   c = EnemyCell::Hit;  break;
      case HitResult::Sunk:
        c = EnemyCell::Sunk;
        g_state.enemy_ships_sunk++;
        // Idealmente segneremmo Sunk anche su tutte le altre celle della nave,
        // ma il server non ce le rivela. Restano Hit finche' non vengono
        // anch'esse marcate. Per la UI bastera' lampeggiare la cella corrente.
        break;
    }
  } else {
    // L'avversario ha sparato sulla mia board.
    OwnCell& c = g_state.own_board[e.x][e.y];
    switch (e.hit) {
      case HitResult::Water:
        // Nota: se la casella era Ship questo non puo' accadere (sarebbe Hit).
        // Quindi era Empty -> diventa Miss (lo segniamo per il renderer).
        if (c == OwnCell::Empty) c = OwnCell::Miss;
        break;
      case HitResult::Hit:
        c = OwnCell::Hit;
        break;
      case HitResult::Sunk:
        c = OwnCell::Sunk;
        g_state.my_ships_sunk++;
        break;
    }
  }
}

bool game_state_aim_can_shoot() {
  if (g_state.cursor_x >= BOARD_W || g_state.cursor_y >= BOARD_H) return false;
  return g_state.enemy_board[g_state.cursor_x][g_state.cursor_y]
         == EnemyCell::Unknown;
}

// ============================================================================
// game_state.h
//
// Stato locale del gioco. Vive nel singleton g_state. Modificato dalla FSM,
// letto dal renderer. NON conosce ne' MQTT ne' display: solo logica.
// ============================================================================
#pragma once

#include "game_config.h"
#include "proto_types.h"
#include "proto_codec.h"
#include <Arduino.h>

// Cosa contiene una cella della MIA flotta.
enum class OwnCell : uint8_t {
  Empty,    // acqua
  Ship,     // mia nave intatta
  Hit,      // mia nave colpita
  Sunk,     // mia nave affondata (segmento di una nave Sunk)
  Miss,     // l'avversario ha sparato qui senza colpire niente
};

// Cosa so della board dell'avversario (dal mio punto di vista).
enum class EnemyCell : uint8_t {
  Unknown,  // non ho ancora sparato qui
  Miss,     // ho sparato e mancato
  Hit,      // ho sparato e colpito (nave ancora viva)
  Sunk,     // ho sparato e affondato
};

struct GameState {
  // --- Identita' di partita ---
  Role     my_role  = Role::None;
  uint32_t game_id  = 0;
  Role     turn     = Role::None;

  // --- Board ---
  OwnCell   own_board  [BOARD_W][BOARD_H];
  EnemyCell enemy_board[BOARD_W][BOARD_H];

  // --- Cursore (usato sia in setup che in mira) ---
  uint8_t cursor_x = 0;
  uint8_t cursor_y = 0;

  // --- Wizard di setup ---
  uint8_t   setup_index = 0;             // prossima nave da piazzare in FLEET_LENS
  Direction setup_dir   = Direction::East;
  Boat      placed_boats[FLEET_COUNT];   // navi gia' confermate

  // --- Conteggio Sunk per dedurre la fine partita (opzionale) ---
  uint8_t my_ships_sunk      = 0;        // navi mie che sono state affondate
  uint8_t enemy_ships_sunk   = 0;        // navi sue che ho affondato
};

extern GameState g_state;

// ---- Cicli di vita ----
void game_state_reset();

// ---- Cursore ----
void game_state_cursor_move(int8_t dx, int8_t dy);

// ---- Setup wizard ----
uint8_t   game_state_setup_current_len();    // 0 se setup completo
Direction game_state_setup_current_dir();
void      game_state_setup_rotate();         // North->East->South->West->North
// Cerca di confermare la nave corrente al cursore con la direzione corrente.
// Ritorna true se la nave era valida ed e' stata aggiunta a placed_boats.
bool      game_state_setup_try_confirm();
bool      game_state_setup_complete();       // true se tutte le navi sono piazzate

// Verifica se una nave (x,y,dir,len) starebbe dentro la griglia e non si
// sovrappone a nessuna nave gia' piazzata.
bool      game_state_setup_valid_placement(uint8_t x, uint8_t y,
                                           Direction dir, uint8_t len);

// Stampa anteprima della nave corrente: scorre le celle che occuperebbe e
// chiama il callback. Utile per il renderer.
void      game_state_setup_foreach_preview_cell(
              void (*cb)(uint8_t x, uint8_t y, void* user), void* user);

// Quando il setup e' completo, prepara il payload per MQTT.
// Scrive in 'out' fino a out_max boats e ritorna quanti ne ha scritti.
size_t    game_state_setup_get_boats(Boat* out, size_t out_max);

// ---- Eventi di gioco ----
// Aggiorna own_board o enemy_board in base all'event ricevuto dal server.
void      game_state_apply_event(const proto::EventMsg& e);

// ---- Mira (fase Playing, mio turno) ----
// True se la casella (cursor_x, cursor_y) sulla board avversaria non e'
// stata ancora sparata: usata per evitare il "doppio shoot" inutile.
bool      game_state_aim_can_shoot();

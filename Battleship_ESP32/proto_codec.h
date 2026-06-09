// ============================================================================
// proto_codec.h
//
// Encode/decode dei messaggi MQTT a/da JSON.
//
// In ricezione: assign, state, event.
// In uscita:    register, action (Shoot / Setup).
//
// Tutte le funzioni di encode scrivono in un buffer fornito dal chiamante e
// ritornano la lunghezza scritta (escluso terminatore) oppure 0 in caso di
// errore. Tutte le decode ritornano true/false.
// ============================================================================
#pragma once

#include "proto_types.h"
#include <stddef.h>

namespace proto {

// ---- Messaggi in ingresso ----
struct AssignMsg {
  Role     role;
  uint32_t game_id;
};

struct StateMsg {
  Role turn;
};

struct EventMsg {
  Role      attacker;
  HitResult hit;
  uint8_t   x;
  uint8_t   y;
};

bool decode_assign(const char* json, size_t len, AssignMsg& out);
bool decode_state (const char* json, size_t len, StateMsg&  out);
bool decode_event (const char* json, size_t len, EventMsg&  out);

// ---- Messaggi in uscita (encode in buffer C-string) ----

// {"id":"AA:BB:CC:DD:EE:FF"}
size_t encode_register(const char* mac, char* out, size_t out_size);

// {"Shoot":[x,y]}
size_t encode_shoot(uint8_t x, uint8_t y, char* out, size_t out_size);

// {"Setup":[ {starting_position:[x,y], direction:"...", len:N}, ... ]}
size_t encode_setup(const Boat* boats, size_t count, char* out, size_t out_size);

} // namespace proto

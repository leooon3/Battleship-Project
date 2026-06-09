// ============================================================================
// proto_types.h
//
// Tipi che rispecchiano quelli del server Rust (game/mod.rs, game/boat/mod.rs,
// game/hit_result.rs) + conversioni a/da stringhe per la serializzazione JSON.
//
// Convenzioni di serializzazione (dal codice Rust):
//   Role        -> lowercase: "host" | "guest"
//   Direction   -> PascalCase: "North" | "East" | "South" | "West"
//   HitResult   -> PascalCase: "Water" | "Hit" | "Sunk"
//
// position e starting_position sono tuple Rust -> array JSON di 2 numeri.
// ============================================================================
#pragma once

#include <Arduino.h>

enum class Role : uint8_t {
  Host,
  Guest,
  None,   // valore sentinella per "non ancora assegnato"
};

enum class Direction : uint8_t {
  North,
  East,
  South,
  West,
};

enum class HitResult : uint8_t {
  Water,
  Hit,
  Sunk,
};

struct Boat {
  uint8_t   x;
  uint8_t   y;
  Direction dir;
  uint8_t   len;
};

// --- Conversioni ---

const char* role_to_str(Role r);
Role        role_from_str(const char* s);

const char* direction_to_str(Direction d);
Direction   direction_from_str(const char* s);

const char* hitresult_to_str(HitResult h);
HitResult   hitresult_from_str(const char* s);

inline Role role_opposite(Role r) {
  return (r == Role::Host) ? Role::Guest : (r == Role::Guest ? Role::Host : Role::None);
}

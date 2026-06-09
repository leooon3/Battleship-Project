// ============================================================================
// proto_types.cpp
//
// Conversioni a/da stringhe per gli enum del protocollo.
// I formati DEVONO combaciare con cio' che produce/accetta Serde sul server.
// ============================================================================
#include "proto_types.h"
#include <string.h>

// ---- Role: lowercase ("host" | "guest") ----
const char* role_to_str(Role r) {
  switch (r) {
    case Role::Host:  return "host";
    case Role::Guest: return "guest";
    default:          return "";
  }
}

Role role_from_str(const char* s) {
  if (!s) return Role::None;
  // il server FromStr accetta sia minuscolo che maiuscolo. Coerenti, controlliamo entrambi.
  if (strcmp(s, "host") == 0  || strcmp(s, "Host") == 0  || strcmp(s, "HOST") == 0)  return Role::Host;
  if (strcmp(s, "guest") == 0 || strcmp(s, "Guest") == 0 || strcmp(s, "GUEST") == 0) return Role::Guest;
  return Role::None;
}

// ---- Direction: PascalCase ("North" | "East" | "South" | "West") ----
const char* direction_to_str(Direction d) {
  switch (d) {
    case Direction::North: return "North";
    case Direction::East:  return "East";
    case Direction::South: return "South";
    case Direction::West:  return "West";
    default:               return "North";
  }
}

Direction direction_from_str(const char* s) {
  if (!s) return Direction::North;
  if (strcmp(s, "North") == 0) return Direction::North;
  if (strcmp(s, "East")  == 0) return Direction::East;
  if (strcmp(s, "South") == 0) return Direction::South;
  if (strcmp(s, "West")  == 0) return Direction::West;
  return Direction::North;
}

// ---- HitResult: PascalCase ("Water" | "Hit" | "Sunk") ----
const char* hitresult_to_str(HitResult h) {
  switch (h) {
    case HitResult::Water: return "Water";
    case HitResult::Hit:   return "Hit";
    case HitResult::Sunk:  return "Sunk";
    default:               return "Water";
  }
}

HitResult hitresult_from_str(const char* s) {
  if (!s) return HitResult::Water;
  if (strcmp(s, "Water") == 0) return HitResult::Water;
  if (strcmp(s, "Hit")   == 0) return HitResult::Hit;
  if (strcmp(s, "Sunk")  == 0) return HitResult::Sunk;
  return HitResult::Water;
}

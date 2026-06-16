// Wire protocol shared with the Rust server: game types + JSON codec.
//
// Serialization must match the server's serde output:
//   Role       lowercase   "host" | "guest"
//   Direction  PascalCase  "North" | "East" | "South" | "West"
//   HitResult  PascalCase  "Water" | "Hit" | "Sunk"
//   positions  JSON arrays [x, y]
#pragma once

#include <Arduino.h>

enum class Role : uint8_t { Host, Guest, None };
enum class Direction : uint8_t { North, East, South, West };
enum class HitResult : uint8_t { Water, Hit, Sunk };

struct Boat {
  uint8_t   x;
  uint8_t   y;
  Direction dir;
  uint8_t   len;
};

const char* role_to_str(Role r);
Role        role_from_str(const char* s);
const char* direction_to_str(Direction d);
const char* hitresult_to_str(HitResult h);

namespace proto {

// Messages received from the server.
struct AssignMsg { Role role; uint32_t game_id; };
struct EventMsg  { Role attacker; HitResult hit; uint8_t x; uint8_t y; };

// The /state topic carries either {"turn": role} during play or
// {"winner": role} on the final move.
struct StateMsg {
  bool over;
  Role turn;     // valid when !over
  Role winner;   // valid when over
};

bool decode_assign(const char* json, size_t len, AssignMsg& out);
bool decode_state (const char* json, size_t len, StateMsg&  out);
bool decode_event (const char* json, size_t len, EventMsg&  out);

// Messages sent to the server. Each writes into the caller's buffer and
// returns the number of bytes written (0 on error).
size_t encode_register(const char* mac, char* out, size_t out_size);
size_t encode_shoot(uint8_t x, uint8_t y, char* out, size_t out_size);
size_t encode_setup(const Boat* boats, size_t count, char* out, size_t out_size);

}  // namespace proto

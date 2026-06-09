// ============================================================================
// proto_codec.cpp
//
// Implementazione encode/decode JSON con ArduinoJson v7.
//
// Riferimento payload (vedi ARCHITECTURE.md sezione 4):
//
//   register  -> {"id":"AA:BB:CC:DD:EE:FF"}
//   assign    -> {"role":"host","game_id":0}
//   state     -> {"turn":"host"}
//   event     -> {"attacker":"host","hit":"Water","position":[x,y]}
//   action    -> {"Shoot":[x,y]}  oppure
//                {"Setup":[{"starting_position":[x,y],"direction":"East","len":2},...]}
// ============================================================================
#include "proto_codec.h"
#include <ArduinoJson.h>

namespace proto {

// ---------------- DECODE ----------------

bool decode_assign(const char* json, size_t len, AssignMsg& out) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json, len);
  if (err) return false;

  const char* role_s = doc["role"] | (const char*)nullptr;
  if (!role_s) return false;
  Role r = role_from_str(role_s);
  if (r == Role::None) return false;

  if (!doc["game_id"].is<uint32_t>() && !doc["game_id"].is<int>()) return false;

  out.role    = r;
  out.game_id = doc["game_id"].as<uint32_t>();
  return true;
}

bool decode_state(const char* json, size_t len, StateMsg& out) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json, len);
  if (err) return false;

  const char* turn_s = doc["turn"] | (const char*)nullptr;
  if (!turn_s) return false;
  Role r = role_from_str(turn_s);
  if (r == Role::None) return false;

  out.turn = r;
  return true;
}

bool decode_event(const char* json, size_t len, EventMsg& out) {
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, json, len);
  if (err) return false;

  const char* atk_s = doc["attacker"] | (const char*)nullptr;
  const char* hit_s = doc["hit"]      | (const char*)nullptr;
  if (!atk_s || !hit_s) return false;

  Role atk = role_from_str(atk_s);
  if (atk == Role::None) return false;

  JsonArrayConst pos = doc["position"].as<JsonArrayConst>();
  if (pos.isNull() || pos.size() < 2) return false;

  out.attacker = atk;
  out.hit      = hitresult_from_str(hit_s);
  out.x        = pos[0].as<uint8_t>();
  out.y        = pos[1].as<uint8_t>();
  return true;
}

// ---------------- ENCODE ----------------

size_t encode_register(const char* mac, char* out, size_t out_size) {
  JsonDocument doc;
  doc["id"] = mac;
  return serializeJson(doc, out, out_size);
}

size_t encode_shoot(uint8_t x, uint8_t y, char* out, size_t out_size) {
  JsonDocument doc;
  JsonArray arr = doc["Shoot"].to<JsonArray>();
  arr.add(x);
  arr.add(y);
  return serializeJson(doc, out, out_size);
}

size_t encode_setup(const Boat* boats, size_t count, char* out, size_t out_size) {
  JsonDocument doc;
  JsonArray setup = doc["Setup"].to<JsonArray>();
  for (size_t i = 0; i < count; i++) {
    JsonObject b = setup.add<JsonObject>();
    JsonArray pos = b["starting_position"].to<JsonArray>();
    pos.add(boats[i].x);
    pos.add(boats[i].y);
    b["direction"] = direction_to_str(boats[i].dir);
    b["len"]       = boats[i].len;
  }
  return serializeJson(doc, out, out_size);
}

} // namespace proto

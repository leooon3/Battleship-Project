#include "protocol.h"
#include <ArduinoJson.h>
#include <string.h>
#include <strings.h>   // strcasecmp

const char* role_to_str(Role r) {
  switch (r) {
    case Role::Host:  return "host";
    case Role::Guest: return "guest";
    default:          return "";
  }
}

Role role_from_str(const char* s) {
  if (!s) return Role::None;
  if (!strcasecmp(s, "host"))  return Role::Host;
  if (!strcasecmp(s, "guest")) return Role::Guest;
  return Role::None;
}

const char* direction_to_str(Direction d) {
  switch (d) {
    case Direction::North: return "North";
    case Direction::East:  return "East";
    case Direction::South: return "South";
    case Direction::West:  return "West";
  }
  return "North";
}

const char* hitresult_to_str(HitResult h) {
  switch (h) {
    case HitResult::Water: return "Water";
    case HitResult::Hit:   return "Hit";
    case HitResult::Sunk:  return "Sunk";
  }
  return "Water";
}

static HitResult hitresult_from_str(const char* s) {
  if (s && !strcmp(s, "Hit"))  return HitResult::Hit;
  if (s && !strcmp(s, "Sunk")) return HitResult::Sunk;
  return HitResult::Water;
}

namespace proto {

bool decode_assign(const char* json, size_t len, AssignMsg& out) {
  JsonDocument doc;
  if (deserializeJson(doc, json, len)) return false;

  Role r = role_from_str(doc["role"] | (const char*)nullptr);
  if (r == Role::None || !doc["game_id"].is<uint32_t>()) return false;

  out.role    = r;
  out.game_id = doc["game_id"].as<uint32_t>();
  return true;
}

bool decode_state(const char* json, size_t len, StateMsg& out) {
  JsonDocument doc;
  if (deserializeJson(doc, json, len)) return false;

  Role r = role_from_str(doc["turn"] | (const char*)nullptr);
  if (r == Role::None) return false;

  out.turn = r;
  return true;
}

bool decode_event(const char* json, size_t len, EventMsg& out) {
  JsonDocument doc;
  if (deserializeJson(doc, json, len)) return false;

  Role attacker = role_from_str(doc["attacker"] | (const char*)nullptr);
  const char* hit = doc["hit"] | (const char*)nullptr;
  JsonArrayConst pos = doc["position"].as<JsonArrayConst>();
  if (attacker == Role::None || !hit || pos.size() < 2) return false;

  out.attacker = attacker;
  out.hit      = hitresult_from_str(hit);
  out.x        = pos[0].as<uint8_t>();
  out.y        = pos[1].as<uint8_t>();
  return true;
}

size_t encode_register(const char* mac, char* out, size_t out_size) {
  JsonDocument doc;
  doc["id"] = mac;
  return serializeJson(doc, out, out_size);
}

size_t encode_shoot(uint8_t x, uint8_t y, char* out, size_t out_size) {
  JsonDocument doc;
  JsonArray xy = doc["Shoot"].to<JsonArray>();
  xy.add(x);
  xy.add(y);
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

}  // namespace proto

// ============================================================================
// net_mqtt.cpp
//
// Wrapper sopra espMqttClient con dispatch sui 3 topic di interesse.
// ============================================================================
#include "net_mqtt.h"
#include "game_config.h"

#include <espMqttClient.h>
#include <string.h>
#include <stdio.h>

// --- Stato ---
static espMqttClient s_mqtt;
static const char*   s_host      = nullptr;
static uint16_t      s_port      = 1883;
static const char*   s_user      = nullptr;
static const char*   s_pass      = nullptr;
static char          s_client_id[24] = {0};  // MAC max 17 + margine
static uint32_t      s_last_attempt_ms = 0;
static bool          s_was_connected = false;

// --- Callbacks utente ---
static OnMqttConnected s_cb_connected = nullptr;
static OnMqttAssign    s_cb_assign    = nullptr;
static OnMqttState     s_cb_state     = nullptr;
static OnMqttEvent     s_cb_event     = nullptr;
static OnMqttRaw       s_cb_raw       = nullptr;

// --- Buffers per payload in uscita ---
// Setup con 4 boats e' il piu' grosso (~280 char), 512 e' confortevole.
static char s_out_buf[512];

// ---------------- Helpers stringhe ----------------

static bool ends_with(const char* s, const char* suffix) {
  size_t ls = strlen(s);
  size_t lsf = strlen(suffix);
  if (lsf > ls) return false;
  return strcmp(s + ls - lsf, suffix) == 0;
}

// Estrae il game_id dal topic "battleship/game/{N}/..."
// Ritorna true se trovato. Si appoggia a strtoul su una porzione tagliata.
static bool extract_game_id_from_topic(const char* topic, uint32_t& out) {
  // cerca "/game/"
  const char* p = strstr(topic, "/game/");
  if (!p) return false;
  p += 6; // dopo "/game/"
  // legge cifre
  char* endp = nullptr;
  unsigned long v = strtoul(p, &endp, 10);
  if (endp == p) return false;
  out = (uint32_t)v;
  return true;
}

// ---------------- Callbacks espMqttClient ----------------

static void onMqttConnect(bool /*sessionPresent*/) {
  Serial.println("[mqtt] connected");
  s_was_connected = true;
  if (s_cb_connected) s_cb_connected();
}

static void onMqttDisconnect(espMqttClientTypes::DisconnectReason reason) {
  Serial.printf("[mqtt] disconnected, reason=%d\n", (int)reason);
  s_was_connected = false;
  s_last_attempt_ms = millis();
}

static void onMqttMessage(const espMqttClientTypes::MessageProperties& /*props*/,
                          const char* topic,
                          const uint8_t* payload,
                          size_t len,
                          size_t index,
                          size_t total) {
  // Payload puo' essere frammentato: per i nostri payload corti non succede,
  // ma stiamo nel safe e ignoriamo le frame intermedie.
  if (index != 0 || len != total) {
    Serial.println("[mqtt] fragmented payload, ignoring (raise buffer if it happens)");
    return;
  }

  // Copia il payload come C-string null-terminated (ArduinoJson accetta anche
  // buffer + len, ma cosi' siamo piu' robusti per i Serial.println di debug).
  static char buf[512];
  if (len >= sizeof(buf)) {
    Serial.printf("[mqtt] payload too big (%u), dropping\n", (unsigned)len);
    return;
  }
  memcpy(buf, payload, len);
  buf[len] = 0;

  Serial.printf("[mqtt] RX topic=%s payload=%s\n", topic, buf);

  // ---- Dispatch ----
  // Sottoscriviamo solo a topic specifici, quindi la classificazione e' per suffisso.
  if (ends_with(topic, "/assign")) {
    proto::AssignMsg m;
    if (proto::decode_assign(buf, len, m)) {
      if (s_cb_assign) s_cb_assign(m);
    } else {
      Serial.println("[mqtt] decode_assign failed");
    }
  }
  else if (ends_with(topic, "/state")) {
    proto::StateMsg m;
    if (proto::decode_state(buf, len, m)) {
      if (s_cb_state) s_cb_state(m);
    } else {
      Serial.println("[mqtt] decode_state failed");
    }
  }
  else if (ends_with(topic, "/event")) {
    proto::EventMsg m;
    if (proto::decode_event(buf, len, m)) {
      if (s_cb_event) s_cb_event(m);
    } else {
      Serial.println("[mqtt] decode_event failed");
    }
  }
  else {
    // Topic non riconosciuto come protocollo di gioco: passa al callback raw
    // se presente, altrimenti logga e basta.
    if (s_cb_raw) {
      s_cb_raw(topic, buf, len);
    } else {
      Serial.printf("[mqtt] unhandled topic: %s\n", topic);
    }
  }
}

// ---------------- API ----------------

void net_mqtt_begin(const char* host, uint16_t port,
                    const char* user, const char* pass,
                    const char* client_id_mac) {
  s_host = host;
  s_port = port;
  s_user = user;
  s_pass = pass;

  // Il client-id MQTT non puo' contenere ':' su tutti i broker, ma Mosquitto
  // lo accetta. Per maggior sicurezza usiamo lo stesso MAC con ':' come da MAC.
  // Se ci sono problemi, sostituire ':' con '_' qui.
  strncpy(s_client_id, client_id_mac, sizeof(s_client_id) - 1);
  s_client_id[sizeof(s_client_id) - 1] = 0;

  s_mqtt.setServer(host, port);
  if (user && user[0]) {
    s_mqtt.setCredentials(user, pass);
  }
  s_mqtt.setClientId(s_client_id);
  s_mqtt.setKeepAlive(MQTT_KEEPALIVE_S);

  s_mqtt.onConnect(onMqttConnect);
  s_mqtt.onDisconnect(onMqttDisconnect);
  s_mqtt.onMessage(onMqttMessage);

  Serial.printf("[mqtt] connecting to %s:%u as %s\n", host, port, s_client_id);
  s_mqtt.connect();
  s_last_attempt_ms = millis();
}

void net_mqtt_loop() {
  // espMqttClient e' async, ma se la connessione cade tenta riconnessione qui.
  if (!s_was_connected && (millis() - s_last_attempt_ms) > MQTT_RETRY_MS) {
    Serial.println("[mqtt] reconnecting...");
    s_mqtt.connect();
    s_last_attempt_ms = millis();
  }
}

bool net_mqtt_is_connected() { return s_was_connected; }

void net_mqtt_set_on_connected(OnMqttConnected cb) { s_cb_connected = cb; }
void net_mqtt_set_on_assign   (OnMqttAssign cb)    { s_cb_assign    = cb; }
void net_mqtt_set_on_state    (OnMqttState cb)     { s_cb_state     = cb; }
void net_mqtt_set_on_event    (OnMqttEvent cb)     { s_cb_event     = cb; }
void net_mqtt_set_on_raw      (OnMqttRaw cb)       { s_cb_raw       = cb; }

// ---------------- Subscriptions / publishes ----------------

bool net_mqtt_subscribe_assign(const char* mac) {
  char topic[64];
  snprintf(topic, sizeof(topic), "battleship/%s/assign", mac);
  uint16_t id = s_mqtt.subscribe(topic, 1);
  Serial.printf("[mqtt] SUB %s (id=%u)\n", topic, id);
  return id != 0;
}

bool net_mqtt_subscribe_game(uint32_t game_id, Role my_role) {
  char topic[80];

  snprintf(topic, sizeof(topic), "battleship/game/%lu/state", (unsigned long)game_id);
  uint16_t id1 = s_mqtt.subscribe(topic, 1);
  Serial.printf("[mqtt] SUB %s (id=%u)\n", topic, id1);

  snprintf(topic, sizeof(topic), "battleship/game/%lu/%s/event",
           (unsigned long)game_id, role_to_str(my_role));
  uint16_t id2 = s_mqtt.subscribe(topic, 1);
  Serial.printf("[mqtt] SUB %s (id=%u)\n", topic, id2);

  return id1 != 0 && id2 != 0;
}

bool net_mqtt_publish_register(const char* mac) {
  size_t n = proto::encode_register(mac, s_out_buf, sizeof(s_out_buf));
  if (n == 0) return false;
  uint16_t id = s_mqtt.publish("battleship/register", 1, false,
                               (const uint8_t*)s_out_buf, n);
  Serial.printf("[mqtt] PUB battleship/register %s (id=%u)\n", s_out_buf, id);
  return id != 0;
}

bool net_mqtt_publish_shoot(uint32_t game_id, Role my_role, uint8_t x, uint8_t y) {
  size_t n = proto::encode_shoot(x, y, s_out_buf, sizeof(s_out_buf));
  if (n == 0) return false;
  char topic[80];
  snprintf(topic, sizeof(topic), "battleship/game/%lu/%s/action",
           (unsigned long)game_id, role_to_str(my_role));
  uint16_t id = s_mqtt.publish(topic, 1, false, (const uint8_t*)s_out_buf, n);
  Serial.printf("[mqtt] PUB %s %s (id=%u)\n", topic, s_out_buf, id);
  return id != 0;
}

bool net_mqtt_publish_setup(uint32_t game_id, Role my_role,
                            const Boat* boats, size_t count) {
  size_t n = proto::encode_setup(boats, count, s_out_buf, sizeof(s_out_buf));
  if (n == 0) return false;
  char topic[80];
  snprintf(topic, sizeof(topic), "battleship/game/%lu/%s/action",
           (unsigned long)game_id, role_to_str(my_role));
  uint16_t id = s_mqtt.publish(topic, 1, false, (const uint8_t*)s_out_buf, n);
  Serial.printf("[mqtt] PUB %s %s (id=%u)\n", topic, s_out_buf, id);
  return id != 0;
}

// ---------------- Raw subscribe / publish ----------------

bool net_mqtt_subscribe_raw(const char* topic, uint8_t qos) {
  uint16_t id = s_mqtt.subscribe(topic, qos);
  Serial.printf("[mqtt] SUB %s qos=%u (id=%u)\n", topic, qos, id);
  return id != 0;
}

bool net_mqtt_publish_raw(const char* topic, const char* payload,
                          uint8_t qos, bool retain) {
  size_t n = payload ? strlen(payload) : 0;
  uint16_t id = s_mqtt.publish(topic, qos, retain,
                               (const uint8_t*)payload, n);
  Serial.printf("[mqtt] PUB %s %s (id=%u)\n", topic,
                payload ? payload : "(null)", id);
  return id != 0;
}

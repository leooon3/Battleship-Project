#include "net_mqtt.h"
#include "game_config.h"

#include <espMqttClient.h>
#include <string.h>

static espMqttClient s_mqtt;
static char          s_client_id[24] = {0};
static uint32_t      s_last_attempt_ms = 0;
static bool          s_connected = false;

static OnMqttConnected s_cb_connected = nullptr;
static OnMqttAssign    s_cb_assign    = nullptr;
static OnMqttState     s_cb_state     = nullptr;
static OnMqttEvent     s_cb_event     = nullptr;

// Outgoing payload buffer. The largest message is a 4-boat Setup (~280 bytes).
static char s_out_buf[512];

static bool ends_with(const char* s, const char* suffix) {
  size_t ls = strlen(s), lf = strlen(suffix);
  return ls >= lf && !strcmp(s + ls - lf, suffix);
}

static void on_connect(bool /*session_present*/) {
  Serial.println("[mqtt] connected");
  s_connected = true;
  if (s_cb_connected) s_cb_connected();
}

static void on_disconnect(espMqttClientTypes::DisconnectReason reason) {
  Serial.printf("[mqtt] disconnected, reason=%d\n", (int)reason);
  s_connected = false;
  s_last_attempt_ms = millis();
}

static void on_message(const espMqttClientTypes::MessageProperties&,
                       const char* topic, const uint8_t* payload,
                       size_t len, size_t index, size_t total) {
  // Our payloads are short and never fragmented; drop anything that is.
  if (index != 0 || len != total) return;

  char buf[512];
  if (len >= sizeof(buf)) {
    Serial.printf("[mqtt] payload too big (%u), dropping\n", (unsigned)len);
    return;
  }
  memcpy(buf, payload, len);
  buf[len] = 0;
  Serial.printf("[mqtt] RX %s %s\n", topic, buf);

  // We only subscribe to assign/state/event, so match on the topic suffix.
  if (ends_with(topic, "/assign")) {
    proto::AssignMsg m;
    if (proto::decode_assign(buf, len, m) && s_cb_assign) s_cb_assign(m);
  } else if (ends_with(topic, "/state")) {
    proto::StateMsg m;
    if (proto::decode_state(buf, len, m) && s_cb_state) s_cb_state(m);
  } else if (ends_with(topic, "/event")) {
    proto::EventMsg m;
    if (proto::decode_event(buf, len, m) && s_cb_event) s_cb_event(m);
  }
}

void net_mqtt_begin(const char* host, uint16_t port,
                    const char* user, const char* pass,
                    const char* client_id_mac) {
  strncpy(s_client_id, client_id_mac, sizeof(s_client_id) - 1);

  s_mqtt.setServer(host, port);
  if (user && user[0]) s_mqtt.setCredentials(user, pass);
  s_mqtt.setClientId(s_client_id);
  s_mqtt.setKeepAlive(MQTT_KEEPALIVE_S);
  s_mqtt.onConnect(on_connect);
  s_mqtt.onDisconnect(on_disconnect);
  s_mqtt.onMessage(on_message);

  Serial.printf("[mqtt] connecting to %s:%u as %s\n", host, port, s_client_id);
  s_mqtt.connect();
  s_last_attempt_ms = millis();
}

void net_mqtt_loop() {
  if (!s_connected && millis() - s_last_attempt_ms > MQTT_RETRY_MS) {
    Serial.println("[mqtt] reconnecting...");
    s_mqtt.connect();
    s_last_attempt_ms = millis();
  }
}

bool net_mqtt_is_connected() { return s_connected; }

void net_mqtt_set_on_connected(OnMqttConnected cb) { s_cb_connected = cb; }
void net_mqtt_set_on_assign   (OnMqttAssign cb)    { s_cb_assign    = cb; }
void net_mqtt_set_on_state    (OnMqttState cb)     { s_cb_state     = cb; }
void net_mqtt_set_on_event    (OnMqttEvent cb)     { s_cb_event     = cb; }

static bool publish_action(uint32_t game_id, Role role, size_t payload_len) {
  if (payload_len == 0) return false;
  char topic[80];
  snprintf(topic, sizeof(topic), "battleship/game/%lu/%s/action",
           (unsigned long)game_id, role_to_str(role));
  uint16_t id = s_mqtt.publish(topic, 1, false, (const uint8_t*)s_out_buf, payload_len);
  Serial.printf("[mqtt] PUB %s %s\n", topic, s_out_buf);
  return id != 0;
}

bool net_mqtt_subscribe_assign(const char* mac) {
  char topic[64];
  snprintf(topic, sizeof(topic), "battleship/%s/assign", mac);
  Serial.printf("[mqtt] SUB %s\n", topic);
  return s_mqtt.subscribe(topic, 1) != 0;
}

bool net_mqtt_subscribe_game(uint32_t game_id, Role my_role) {
  char topic[80];
  snprintf(topic, sizeof(topic), "battleship/game/%lu/state", (unsigned long)game_id);
  uint16_t id1 = s_mqtt.subscribe(topic, 1);
  Serial.printf("[mqtt] SUB %s\n", topic);

  snprintf(topic, sizeof(topic), "battleship/game/%lu/%s/event",
           (unsigned long)game_id, role_to_str(my_role));
  uint16_t id2 = s_mqtt.subscribe(topic, 1);
  Serial.printf("[mqtt] SUB %s\n", topic);

  return id1 != 0 && id2 != 0;
}

bool net_mqtt_publish_register(const char* mac) {
  size_t n = proto::encode_register(mac, s_out_buf, sizeof(s_out_buf));
  if (n == 0) return false;
  uint16_t id = s_mqtt.publish("battleship/register", 1, false,
                               (const uint8_t*)s_out_buf, n);
  Serial.printf("[mqtt] PUB battleship/register %s\n", s_out_buf);
  return id != 0;
}

bool net_mqtt_publish_shoot(uint32_t game_id, Role my_role, uint8_t x, uint8_t y) {
  return publish_action(game_id, my_role,
                        proto::encode_shoot(x, y, s_out_buf, sizeof(s_out_buf)));
}

bool net_mqtt_publish_setup(uint32_t game_id, Role my_role,
                            const Boat* boats, size_t count) {
  return publish_action(game_id, my_role,
                        proto::encode_setup(boats, count, s_out_buf, sizeof(s_out_buf)));
}

// ============================================================================
// hello.cpp
// ============================================================================
#include "hello.h"
#include "net_mqtt.h"
#include <string.h>
#include <stdio.h>

static char     s_mac[24]       = {0};
static char     s_topic_out[80] = {0};
static char     s_topic_in[80]  = {0};
static uint32_t s_last_pub_ms   = 0;
static uint32_t s_pub_counter   = 0;
static bool     s_initialized   = false;

void hello_init(const char* mac) {
  strncpy(s_mac, mac, sizeof(s_mac) - 1);
  s_mac[sizeof(s_mac) - 1] = 0;

  // Topic specifico di questa scheda per i messaggi che pubblichiamo noi.
  snprintf(s_topic_out, sizeof(s_topic_out),
           "battleship/hello/%s/out", s_mac);

  // Topic specifico di questa scheda per i messaggi che riceviamo (l'unico
  // canale via cui il broker puo' parlare a NOI in particolare).
  snprintf(s_topic_in, sizeof(s_topic_in),
           "battleship/hello/%s/in", s_mac);

  s_initialized = true;
  Serial.printf("[hello] init: out=%s, in=%s\n", s_topic_out, s_topic_in);
}

void hello_on_mqtt_connected() {
  if (!s_initialized) {
    Serial.println("[hello] WARN: connected before init, skipping");
    return;
  }

  // Sottoscrivi al topic "in" specifico di questa scheda.
  net_mqtt_subscribe_raw(s_topic_in, 1);

  // Primo pub immediato.
  char payload[160];
  snprintf(payload, sizeof(payload),
           "{\"mac\":\"%s\",\"msg\":\"hello\",\"uptime_ms\":%lu,\"n\":%lu}",
           s_mac, (unsigned long)millis(), (unsigned long)s_pub_counter++);
  net_mqtt_publish_raw(s_topic_out, payload, 1, false);
  s_last_pub_ms = millis();
}

void hello_on_raw_message(const char* topic, const char* payload, size_t len) {
  if (!topic) return;

  // Ci interessano solo i topic che iniziano con "battleship/hello/".
  const char* prefix = "battleship/hello/";
  if (strncmp(topic, prefix, strlen(prefix)) != 0) {
    return; // non e' un hello, lascia stare
  }

  Serial.printf("[hello] RX topic=%s len=%u payload=%.*s\n",
                topic, (unsigned)len, (int)len, payload);
}

void hello_loop() {
  if (!s_initialized) return;

  // Rileva edge MQTT-disconnect -> connect, cosi' non serve registrare un
  // callback dedicato in net_mqtt (che ne ha solo uno e lo usa la FSM).
  static bool was_connected = false;
  bool connected = net_mqtt_is_connected();
  if (connected && !was_connected) {
    hello_on_mqtt_connected();
  }
  was_connected = connected;
  if (!connected) return;

  uint32_t now = millis();
  if (now - s_last_pub_ms < HELLO_PERIOD_MS) return;

  char payload[160];
  snprintf(payload, sizeof(payload),
           "{\"mac\":\"%s\",\"msg\":\"hello\",\"uptime_ms\":%lu,\"n\":%lu}",
           s_mac, (unsigned long)now, (unsigned long)s_pub_counter++);
  net_mqtt_publish_raw(s_topic_out, payload, 1, false);
  s_last_pub_ms = now;
}

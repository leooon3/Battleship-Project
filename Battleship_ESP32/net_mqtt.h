// ============================================================================
// net_mqtt.h
//
// Wrapper sopra espMqttClient. Gestisce connessione al broker, sottoscrizione
// ai topic, dispatch dei payload ai callback registrati, publish degli action.
//
// I topic vengono costruiti internamente in base al MAC e ai parametri di
// gioco (game_id, role) per evitare errori di stringa nel chiamante.
// ============================================================================
#pragma once

#include "proto_codec.h"
#include "proto_types.h"
#include <Arduino.h>

// --- Callback types ---
using OnMqttConnected = void (*)();
using OnMqttAssign    = void (*)(const proto::AssignMsg&);
using OnMqttState     = void (*)(const proto::StateMsg&);
using OnMqttEvent     = void (*)(const proto::EventMsg&);
// Callback "raw" per topic che non rientrano nel protocollo di gioco
// (es. test, telemetria, hello). Topic e payload sono validi solo
// durante la chiamata; copiali se devi conservarli.
using OnMqttRaw       = void (*)(const char* topic,
                                 const char* payload, size_t len);

// --- Init / loop ---
void net_mqtt_begin(const char* host, uint16_t port,
                    const char* user, const char* pass,
                    const char* client_id_mac);
void net_mqtt_loop();
bool net_mqtt_is_connected();

// --- Callback registration ---
void net_mqtt_set_on_connected(OnMqttConnected cb);
void net_mqtt_set_on_assign   (OnMqttAssign cb);
void net_mqtt_set_on_state    (OnMqttState cb);
void net_mqtt_set_on_event    (OnMqttEvent cb);
void net_mqtt_set_on_raw      (OnMqttRaw cb);

// --- Subscriptions / publishes (alto livello) ---
bool net_mqtt_subscribe_assign(const char* mac);
bool net_mqtt_subscribe_game(uint32_t game_id, Role my_role);
bool net_mqtt_publish_register(const char* mac);
bool net_mqtt_publish_shoot(uint32_t game_id, Role my_role, uint8_t x, uint8_t y);
bool net_mqtt_publish_setup(uint32_t game_id, Role my_role,
                            const Boat* boats, size_t count);

// --- Subscribe / publish RAW (per topic non di protocollo, es. hello) ---
bool net_mqtt_subscribe_raw(const char* topic, uint8_t qos = 1);
bool net_mqtt_publish_raw(const char* topic, const char* payload,
                          uint8_t qos = 1, bool retain = false);

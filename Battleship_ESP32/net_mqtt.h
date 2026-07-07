// MQTT client wrapper around espMqttClient. Builds topic strings internally
// from the MAC, game_id and role, decodes incoming payloads and hands them to
// the registered callbacks
#pragma once

#include "protocol.h"
#include <Arduino.h>

using OnMqttConnected = void (*)();
using OnMqttAssign    = void (*)(const proto::AssignMsg&);
using OnMqttState     = void (*)(const proto::StateMsg&);
using OnMqttEvent     = void (*)(const proto::EventMsg&);

void net_mqtt_begin(const char* host, uint16_t port,
                    const char* user, const char* pass,
                    const char* client_id_mac);
void net_mqtt_loop();

void net_mqtt_set_on_connected(OnMqttConnected cb);
void net_mqtt_set_on_assign   (OnMqttAssign cb);
void net_mqtt_set_on_state    (OnMqttState cb);
void net_mqtt_set_on_event    (OnMqttEvent cb);

bool net_mqtt_subscribe_assign(const char* mac);
bool net_mqtt_subscribe_game(uint32_t game_id, Role my_role);
bool net_mqtt_publish_register(const char* mac);
bool net_mqtt_publish_shoot(uint32_t game_id, Role my_role, uint8_t x, uint8_t y);
bool net_mqtt_publish_setup(uint32_t game_id, Role my_role,
                            const Boat* boats, size_t count);

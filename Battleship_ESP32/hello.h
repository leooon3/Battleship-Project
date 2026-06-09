// ============================================================================
// hello.h
//
// Mini-modulo di "ping" MQTT per testare il broker end-to-end senza dipendere
// dal server di gioco. Vive in parallelo alla FSM: usa solo net_mqtt e non
// tocca game_state.
//
// Topics:
//   battleship/hello/{MAC}/out   <- ESP32 pubblica ogni HELLO_PERIOD_MS
//   battleship/hello/+/in        <- ESP32 sottoscrive: il broker puo' parlare
//                                   a questa scheda inviando qui un payload
//                                   (es. da mosquitto_pub)
//
// Esempio test dal Pi:
//   mosquitto_sub -h 192.168.4.1 -t "battleship/hello/#" -v
//   mosquitto_pub -h 192.168.4.1 -t "battleship/hello/AA:BB:.../in" -m "ciao"
// ============================================================================
#pragma once

#include <Arduino.h>

// Periodo di pubblicazione dell'hello "outgoing".
constexpr uint32_t HELLO_PERIOD_MS = 5000;

// Da chiamare una volta dopo che si conosce il MAC (es. dopo net_wifi_begin).
void hello_init(const char* mac);

// Da chiamare quando MQTT diventa connesso. Sottoscrive e fa il primo pub.
void hello_on_mqtt_connected();

// Da chiamare quando arriva un messaggio "raw" da net_mqtt. Filtra solo i
// topic hello e logga; ignora il resto.
void hello_on_raw_message(const char* topic, const char* payload, size_t len);

// Loop cooperativo: pubblica heartbeat ogni HELLO_PERIOD_MS.
void hello_loop();

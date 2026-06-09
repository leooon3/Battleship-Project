// ============================================================================
// game_config.h
//
// Costanti del gioco e della rete. Modificabili senza toccare la logica.
// Tutto cio' che riguarda display / lettura joystick (brightness, deadzone,
// debouncing, ecc.) NON sta qui: lo gestisce l'altro team nei loro file.
// ============================================================================
#pragma once

#include <Arduino.h>

// --- Dimensione board (combacia col default del server Rust: 8x8) ---
constexpr int BOARD_W = 8;
constexpr int BOARD_H = 8;

// --- Flotta demo concordata: 3 navi da 1 + 1 nave da 2 ---
// Ordine = ordine in cui il wizard di setup le propone al giocatore.
// Mettiamo prima la piu' lunga, e' piu' comoda da piazzare per prima.
constexpr int FLEET_LENS[] = { 2, 1, 1, 1 };
constexpr int FLEET_COUNT  = sizeof(FLEET_LENS) / sizeof(FLEET_LENS[0]);

// --- Net / MQTT ---
constexpr uint32_t WIFI_RETRY_MS      = 2000;
constexpr uint32_t MQTT_RETRY_MS      = 2000;
constexpr uint16_t MQTT_KEEPALIVE_S   = 15;

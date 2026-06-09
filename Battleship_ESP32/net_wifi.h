// ============================================================================
// net_wifi.h
//
// Wrapper sottile sopra WiFi.h del core Arduino-ESP32. Connessione station
// con auto-reconnect e accesso al MAC formattato (canonico "AA:BB:CC:DD:EE:FF").
// ============================================================================
#pragma once

#include <Arduino.h>

void        net_wifi_begin(const char* ssid, const char* pass);
void        net_wifi_loop();
bool        net_wifi_is_connected();

// Restituisce il MAC come stringa "AA:BB:CC:DD:EE:FF" (uppercase, con due punti).
// Puntatore a buffer interno statico — valido per tutta la vita del programma.
const char* net_wifi_mac();

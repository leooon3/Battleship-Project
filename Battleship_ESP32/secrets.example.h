// ============================================================================
// secrets.example.h
//
// Copia questo file come "secrets.h" nella stessa cartella e compila i valori
// reali. "secrets.h" e' in .gitignore: i valori veri non finiscono mai in git.
// ============================================================================
#pragma once

// WiFi: l'AP creato dal Raspberry Pi
#define WIFI_SSID  "battleship-pi"
#define WIFI_PASS  "your_wifi_password"

// MQTT broker: Mosquitto sul Raspberry Pi
#define MQTT_HOST  "192.168.4.1"   // IP del Pi sull'interfaccia AP
#define MQTT_PORT  1883            // dal main.rs del server

// Se il broker richiede autenticazione, definire questi due
// (lasciare commentati per anonimo, che e' il default del server attuale):
// #define MQTT_USER  "..."
// #define MQTT_PASS  "..."

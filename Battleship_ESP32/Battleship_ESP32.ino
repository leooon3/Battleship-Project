// ============================================================================
// Battleship_ESP32.ino
//
// Sketch principale. Init dei moduli e loop cooperativo.
//
// Scope di QUESTO firmware (vedi ARCHITECTURE.md):
//   - WiFi + connessione MQTT
//   - Codifica/decodifica protocollo del server Rust (JSON)
//   - Macchina a stati di partita (register -> assign -> setup -> play)
//   - Stato locale del gioco (board, cursore, navi affondate)
//   - Modulo "hello" per test/debug del broker
//
// NON sta qui (lo fa l'altro team nello stesso sketch):
//   - Lettura joystick (debounce, ADC, ...) -> chiama app_on_input() (hal.h)
//   - Driver matrice LED -> legge g_state e disegna come preferisce
//
// Per compilare:
//   1. Aprire questa cartella con Arduino IDE 2.x
//   2. Installare le librerie (Library Manager):
//        - espMqttClient (Bert Melis)
//        - AsyncTCP (ESP32Async)        - dipendenza di espMqttClient async
//        - ArduinoJson v7
//   3. Copiare secrets.example.h come secrets.h e compilare i valori
//   4. Tools -> Board -> ESP32 WROOM-DA Module (o board equivalente)
//   5. Tools -> Port -> COM della scheda
//   6. Upload, poi Serial Monitor a 115200 baud
// ============================================================================
#include "secrets.h"
#include "game_config.h"

#include "net_wifi.h"
#include "net_mqtt.h"
#include "app_fsm.h"

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println();
  Serial.println("===========================================");
  Serial.println(" Battleship ESP32 firmware (bridge)");
  Serial.println("===========================================");

  // --- WiFi ---
  net_wifi_begin(WIFI_SSID, WIFI_PASS);

  // Aspettiamo che WiFi sia su prima di tentare MQTT (max 15s).
  Serial.println("[boot] waiting for WiFi before starting MQTT...");
  uint32_t t0 = millis();
  while (!net_wifi_is_connected() && (millis() - t0) < 15000) {
    net_wifi_loop();
    delay(100);
  }
  if (!net_wifi_is_connected()) {
    Serial.println("[boot] WiFi non up dopo 15s, avvio MQTT comunque (retry interno)");
  }

  // --- MQTT ---
#ifdef MQTT_USER
  net_mqtt_begin(MQTT_HOST, MQTT_PORT, MQTT_USER, MQTT_PASS, net_wifi_mac());
#else
  net_mqtt_begin(MQTT_HOST, MQTT_PORT, nullptr, nullptr, net_wifi_mac());
#endif

  // --- FSM ---
  app_fsm_begin();

  Serial.println("[boot] setup() done");
}

void loop() {
  net_wifi_loop();
  net_mqtt_loop();
  app_fsm_loop();

  // Cede CPU senza bloccare per troppo tempo.
  delay(2);
}

# Battleship ESP32 — Firmware Architecture

Documento di design del firmware dell'ESP32. Sul firmware lavorano **due gruppi**
che condividono lo stesso sketch Arduino:

- **Questo gruppo (noi)**: bridge MQTT verso il server Raspberry Pi, protocollo
  JSON, macchina a stati di partita, stato locale del gioco.
- **Altro gruppo**: driver matrice LED + lettura joystick, sui pin dell'ESP32.

Il confine tra i due lati è descritto nella [§7](#7-confine-coi-moduli-display-joystick).

---

## 1. Topologia

Ogni ESP32 = 1 giocatore. Firmware identico su ogni unità; il MAC differenzia.
Il Raspberry Pi fa da Access Point WiFi (`dawspie` su 2.4 GHz) e da broker
MQTT (Mosquitto su `192.168.4.1:1883`). Il server di gioco è scritto in Rust e
si collega a Mosquitto come un altro client MQTT.

```
   Player A                                         Player B
  ┌─────────┐                                      ┌─────────┐
  │ matrix  │                                      │ matrix  │
  │ 8x8 RGB │                                      │ 8x8 RGB │
  └────┬────┘                                      └────┬────┘
       │ GPIO (gestione: altro team)                    │
  ┌────┴────┐    WiFi    ┌──────────────┐    WiFi  ┌────┴────┐
  │ ESP32_A ├────────────┤  Raspberry   ├──────────┤ ESP32_B │
  └────┬────┘            │   Pi (AP)    │          └────┬────┘
       │ ADC             │ Mosquitto    │               │
  ┌────┴────┐            │ + Rust game  │          ┌────┴────┐
  │joystick │            │   server     │          │joystick │
  └─────────┘            └──────────────┘          └─────────┘
```

---

## 2. Hardware

- **Board**: ESP32 **WROOM-DA Module** (classico ESP32, USB-Seriale via CP210x/CH340)
- **Matrice 8×8 RGB** (WS2812B-compatibile): collegata via GPIO, gestita dall'altro team
- **Joystick analogico** (2 assi + pulsante): collegato via ADC1 + GPIO, gestito dall'altro team

Il nostro codice **non legge ADC e non scrive sui LED**. Riceve eventi input
via `app_on_input(InputEvent)` e mette tutto lo stato di gioco in `g_state`
(struct globale in `game_state.h`) da cui l'altro team legge per disegnare.

---

## 3. Stack software

| Aspetto | Scelta |
|---|---|
| IDE | Arduino IDE 2.x |
| Core | Arduino-ESP32 3.x |
| MQTT | `espMqttClient` (Bert Melis) + `AsyncTCP` (ESP32Async) |
| JSON | `ArduinoJson` v7.x |
| WiFi | `WiFi.h` (incluso nel core) |

### Installazione librerie (Tools → Manage Libraries)

1. `espMqttClient` di Bert Melis
2. `AsyncTCP` di ESP32Async (dipendenza di espMqttClient async)
3. `ArduinoJson` v7.x di Benoit Blanchon

### Boards Manager

URL custom da aggiungere in Preferences:
```
https://espressif.github.io/arduino-esp32/package_esp32_index.json
```
Poi installare **esp32 by Espressif Systems**.

### Selezione board

- Tools → Board → esp32 → **ESP32 WROOM-DA Module**
- Upload Speed: 921600
- Flash Size: 4MB
- Partition Scheme: Default 4MB with spiffs
- CPU: 240 MHz

---

## 4. Wire protocol (lato Rust del server)

### Topic — ESP32 pubblica

| Topic | Quando | Payload JSON |
|---|---|---|
| `battleship/register` | dopo MQTT connect | `{"id":"AA:BB:CC:DD:EE:FF"}` |
| `battleship/game/{gid}/{role}/action` | durante gioco | `{"Shoot":[x,y]}` o `{"Setup":[...]}` |

### Topic — ESP32 sottoscrive

| Topic | Quando | Payload JSON |
|---|---|---|
| `battleship/{MAC}/assign` | dopo register | `{"role":"host","game_id":0}` |
| `battleship/game/{gid}/state` | dopo assign | `{"turn":"host"}` |
| `battleship/game/{gid}/{role}/event` | dopo assign | `{"attacker":"host","hit":"Water","position":[x,y]}` |

### Convenzioni di serializzazione (dal codice Rust)

| Campo | Formato |
|---|---|
| `role` | lowercase: `"host"`, `"guest"` |
| `direction` | PascalCase: `"North"`, `"East"`, `"South"`, `"West"` |
| `hit` | PascalCase: `"Water"`, `"Hit"`, `"Sunk"` |
| `position` / `starting_position` | array JSON di 2 numeri `[x, y]` |
| `Action` | enum esternamente taggato: `{"Shoot":[x,y]}`, `{"Setup":[...]}` |

---

## 5. Macchina a stati

```
       ┌────────┐
       │  BOOT  │  init Serial, WiFi, MQTT, FSM
       └───┬────┘
           ▼
   ┌────────────────┐
   │ WAITINGNET     │   WiFi + MQTT non ancora connessi
   └───┬────────────┘
       ▼
   ┌────────────────┐
   │  REGISTERING   │   sub battleship/{MAC}/assign
   │                │   pub battleship/register {"id":MAC}
   └───┬────────────┘
       ▼ ricevuto assign(role, game_id)
       │ sub battleship/game/{gid}/state e .../{role}/event
   ┌────────────────┐
   │     SETUP      │   wizard: per ogni nave in FLEET_LENS
   │                │     - InputEvent::Up/Down/Left/Right → muovi cursore
   │                │     - InputEvent::BtnShort → ruota direzione
   │                │     - InputEvent::BtnLong  → conferma piazzamento
   │                │   alla fine pub Action::Setup(boats)
   └───┬────────────┘
       ▼
   ┌────────────────────┐
   │ WAITINGGAMESTART   │   attende state(turn) dal server
   └───┬────────────────┘
       ▼
   ┌────────────────┐
   │    PLAYING     │
   │                │   - se turn == mio: cursore mira sul campo avversario,
   │                │     BtnShort → pub Action::Shoot
   │                │   - se turn != mio: input ignorato
   │                │   - on event: aggiorno own_board o enemy_board
   │                │   - on state: aggiorno turno
   └────────────────┘
```

Note:
- **Riconnessione**: se cade WiFi o MQTT, retry interni. Dopo riconnessione si
  rifà `register`, accettando che parta una nuova partita.
- **Game over**: il server non manda un evento esplicito di fine. Per ora il
  firmware resta in `PLAYING` e conta `my_ships_sunk` / `enemy_ships_sunk`
  per uso del renderer (vittoria/sconfitta dedotte).

---

## 6. Layout moduli

Tutti i file in `Battleship_ESP32/` (vincolo Arduino IDE: niente sub-folder).

| File | Responsabilità |
|---|---|
| `Battleship_ESP32.ino` | `setup()` + `loop()` |
| `secrets.h` | credenziali WiFi + indirizzo broker. **Non versionato** |
| `secrets.example.h` | template versionato |
| `game_config.h` | `BOARD_W`, `BOARD_H`, `FLEET_LENS`, timing rete |
| `protocol.h/.cpp` | tipi del protocollo (`Role`, `Direction`, `HitResult`, `Boat`) + encode/decode JSON via ArduinoJson |
| `net_wifi.h/.cpp` | connessione WiFi (DHCP) + MAC dagli eFuses |
| `net_mqtt.h/.cpp` | wrapper espMqttClient: connect, sub, pub, dispatch verso callback |
| `game_state.h/.cpp` | singleton `g_state`: board, cursore, wizard setup, contatori |
| `app_fsm.h/.cpp` | FSM + handler input + callback MQTT |
| `hal.h` | interfaccia input dal team joystick (`app_on_input(InputEvent)`) |

---

## 7. Confine coi moduli display + joystick

Il punto di contatto col secondo team è **minimale e simmetrico**:

### Loro → noi: input
Il team joystick chiama:
```cpp
void app_on_input(InputEvent e);   // dichiarata in hal.h
```
con eventi discreti: `Up`, `Down`, `Left`, `Right`, `BtnShort`, `BtnLong`.
Quando rilevano un movimento o un click, ci chiamano. Sta a loro fare deadzone,
debounce, repeat-rate.

### Noi → loro: stato di gioco
Il team display legge **direttamente** il singleton `g_state` (in `game_state.h`):
- `g_state.my_role`, `g_state.turn`, `g_state.game_id`
- `g_state.own_board[x][y]` (`OwnCell::Empty|Ship|Hit|Sunk|Miss`)
- `g_state.enemy_board[x][y]` (`EnemyCell::Unknown|Miss|Hit|Sunk`)
- `g_state.cursor_x`, `g_state.cursor_y`
- `g_state.setup_index`, `g_state.placed_boats[]` (per anteprima setup)
- `g_state.my_ships_sunk`, `g_state.enemy_ships_sunk`

In più possono usare `app_fsm_phase()` per sapere in che fase siamo (Setup,
Playing, ...). Da queste info disegnano come preferiscono (color scheme,
animazioni, pulsazioni cursore...).

### Cosa NON facciamo noi
- NO `digitalWrite` / `analogRead`
- NO `FastLED` / `NeoPixel`
- NO scelta dei pin
- NO render

---

## 8. Configurazione (`secrets.h`)

```cpp
#define WIFI_SSID  "dawspie"
#define WIFI_PASS  "p4ssw0rd!"
#define MQTT_HOST  "192.168.4.1"   // o "dawspie" se mDNS risolve
#define MQTT_PORT  1883
// se il broker richiede auth:
// #define MQTT_USER  "..."
// #define MQTT_PASS  "..."
```

`secrets.h` è in `.gitignore`. Versionato solo `secrets.example.h`.

### IP statico (workaround DHCP Pi)

Il DHCP server di NetworkManager sul Pi attualmente non assegna IP ai client.
In `net_wifi.cpp` forziamo IP statico `192.168.4.150`. Cambia il quarto ottetto
se più ESP32 girano sulla stessa rete. Quando il DHCP del Pi verrà fixato,
commenta le 4 righe `WiFi.config(...)` in `net_wifi_begin()`.

---

## 9. Debug col broker

Per vedere tutto il traffico verso/dal broker (utile per debug del protocollo):
```bash
mosquitto_sub -h <broker> -t "battleship/#" -v
```

Per pubblicare a mano un messaggio (es. simulare un Setup o uno Shoot senza
joystick), evita il quoting hell di PowerShell: scrivi il JSON in un file e
usa `mosquitto_pub -f file.json`. Esempio:
```powershell
$body = '{"Shoot":[1,1]}'
Set-Content -Path shoot.json -Value $body -Encoding ASCII -NoNewline
mosquitto_pub -h 127.0.0.1 -t "battleship/game/0/guest/action" -f shoot.json
```

---

## 10. Open points

- DHCP del Pi non assegna lease → workaround IP statico (vedi §8)
- Server Rust non manda evento di fine partita → game over dedotto contando i `Sunk`
- Validazione setup minima (dentro griglia + no overlap); regole più rigide del
  battleship classico (no navi adiacenti) non sono imposte

# Guida per il team Interface 👋

Come collegare il vostro codice (joystick + matrice LED) al nostro (rete + MQTT +
server) tramite una **facciata sincrona**: voi scrivete un normale programma
lineare/bloccante — quello che già volevate — e noi gestiamo tutta la rete dietro.

---

## TL;DR

1. Chiamate le funzioni **`battle_*`** (in `battle.h`) in ordine, come un programma
   bloccante normale. Ognuna ritorna quando il server ha risposto.
2. Leggete **`g_state`** (`game_state.h`) per sapere cosa disegnare.
3. Voi fate **tutto** l'input (joystick) e l'output (matrice). Di WiFi, MQTT, JSON,
   turni, server **non vi occupate**: sta tutto dietro le `battle_*`.

La rete gira su un **core separato**, quindi mentre voi bloccate (attesa tasto,
attesa avversario…) la connessione **non cade mai**.

---

## Dove lavorare

```bash
git clone https://github.com/leooon3/Battleship-Project.git
cd Battleship-Project
git checkout interface
```

Tutto lo sketch vive in `Battleship_ESP32/`. Voi aggiungete i vostri file
(`display.cpp`, `joystick.cpp`, `pins.h`, …) e scrivete il `.ino` principale che
usa le `battle_*`. I nostri file (`battle.*`, `net_*`, `protocol.*`, `game_state.*`)
non li toccate.

---

## 1️⃣ La facciata — `battle.h`

```cpp
void      battle_begin();                            // connette (blocca finché MQTT è su)
Role      battle_register();                          // register + attende il ruolo assegnato
void      battle_send_setup(const Boat* boats, size_t n); // invia la flotta + attende inizio partita
bool      battle_my_turn();                           // NON bloccante: è il mio turno?
bool      battle_over();                              // NON bloccante: partita finita?
Role      battle_winner();                            // valido dopo battle_over()
HitResult battle_shoot(uint8_t x, uint8_t y);         // spara + attende l'esito
void      battle_await_change();                      // blocca finché torna il mio turno / fine partita
HitResult battle_wait_for_opponent_shot(uint8_t& out_x, uint8_t& out_y); // blocca finché l'avversario non spara, restituisce coord ed esito
```

Le funzioni **bloccanti** ritornano solo quando il round-trip col server è finito.
Quando `battle_shoot` ritorna, le board in `g_state` sono **già aggiornate**.

---

## 2️⃣ Esempio completo (il vostro `.ino`)

Tutto si legge come un programma sincrono:

```cpp
#include "battle.h"
#include "game_state.h"
#include "game_config.h"
// + le vostre lib (FastLED, ecc.)

void setup() {
  interface_hw_init();          // VOSTRO: FastLED, pin joystick
  battle_begin();               // NOSTRO: connette (blocca; intanto mostrate "connessione")
}

void loop() {
  interface_show_waiting();     // VOSTRO: schermata barchette
  interface_wait_button();      // VOSTRO: aspettate il pulsante (bloccante, come già fate)

  Role me = battle_register();  // NOSTRO: pairing col server (blocca)

  Boat boats[FLEET_COUNT];
  size_t n = interface_place_ships(boats);   // VOSTRO: il vostro setup, riempie boats[]
  battle_send_setup(boats, n);  // NOSTRO: invia + attende che parta la partita (blocca)

  while (!battle_over()) {
    if (battle_my_turn()) {
      uint8_t x, y;
      interface_aim(&x, &y);              // VOSTRO: mira col joystick
      HitResult r = battle_shoot(x, y);   // NOSTRO: spara + esito (blocca)
      interface_show_shot(x, y, r);       // VOSTRO: disegnate l'esito
    } else {
      uint8_t ox, oy;
      HitResult r = battle_wait_for_opponent_shot(ox, oy); // NOSTRO: blocca finché l'avversario spara
      interface_show_opponent_shot(ox, oy, r);  // VOSTRO: disegnate il colpo subito
    }
  }

  interface_show_result(battle_winner()); // VOSTRO: vittoria/sconfitta
}
```

> ⚠️ **Mentre una `battle_*` blocca, il vostro `loop()` è fermo lì**: nessuna
> animazione gira durante l'attesa. Per un gioco a turni va bene (disegnate lo
> stato, poi bloccate). Se volete animazioni *durante* le attese (es. un'onda
> mentre aspettate l'avversario), ditecelo: si fa con un task display separato.

---

## 3️⃣ Cosa leggete per disegnare — `game_state.h`

```cpp
extern GameState g_state;

struct GameState {
  Role     my_role;                          // Host / Guest
  OwnCell   own_board  [BOARD_W][BOARD_H];   // la VOSTRA flotta + colpi subiti
  EnemyCell enemy_board[BOARD_W][BOARD_H];   // quello che sapete dell'avversario
  uint8_t my_ships_sunk;                     // 0..N (progresso, per il display)
  uint8_t enemy_ships_sunk;                  // 0..N
};
```

### Significato delle celle

```cpp
enum class OwnCell {
  Empty,  // acqua
  Ship,   // vostra nave intatta
  Hit,    // vostra nave colpita
  Sunk,   // vostra nave affondata
  Miss,   // l'avversario ha sparato qui a vuoto
};

enum class EnemyCell {
  Unknown,  // non avete ancora sparato qui
  Miss,     // sparato e mancato
  Hit,      // colpito (nave ancora viva)
  Sunk,     // affondata
};
```

Le board si aggiornano da sole (le scriviamo noi quando arriva un evento dal
server). Voi le leggete e basta.

### La fase corrente (per decidere COSA disegnare)

📂 `app_fsm.h`
```cpp
enum class AppPhase {
  Init, WaitingNet, Ready, Registering, SettingUp, WaitingGameStart, Playing, End
};
AppPhase app_fsm_phase();     // fase corrente, aggiornata da noi
```

Se preferite pilotare il display in base a una **fase** (invece di seguire il
flusso lineare delle `battle_*`), leggete `app_fsm_phase()`: è la stessa
informazione, la teniamo aggiornata noi. Esempio:
```cpp
switch (app_fsm_phase()) {
  case AppPhase::Ready:    draw_waiting();  break;   // barchette, attesa start
  case AppPhase::SettingUp:draw_setup();    break;
  case AppPhase::Playing:  draw_boards();   break;
  case AppPhase::End:      draw_result();   break;
  default:                 draw_connecting();break;
}
```

---

## 4️⃣ Le navi del setup — `Boat`

Il vostro codice di piazzamento deve riempire un array (o vector) di `Boat` e
passarlo a `battle_send_setup`. La flotta richiesta è in `FLEET_LENS`
(`game_config.h`). Il tipo `Boat` e l'`enum Direction` sono definiti in
`protocol.h` (incluso da `battle.h`) e sono **identici** ai vostri:

```cpp
enum Direction { NORTH, EAST, SOUTH, WEST };

struct Boat {
  int       len;            // lunghezza
  Direction dir;            // NORTH / EAST / SOUTH / WEST
  int       initial_pos[2]; // [x, y] di partenza
};
```

Esempio (una nave da 4 verso est + una da 2 verso nord):
```cpp
boats[0] = { 4, EAST,  {0, 0} };  // (0,0)-(3,0)
boats[1] = { 2, NORTH, {3, 5} };  // (3,5)-(3,6)
```

Il **modello di piazzamento** (come il giocatore sceglie le navi col joystick) è
**vostro** — potete tenere il vostro setup a trascinamento. Alla fine ci passate
solo l'array `boats[]`.

---

## 5️⃣ Coordinate

- Board 8×8: `x ∈ [0..7]`, `y ∈ [0..7]`
- `x=0` a sinistra, `x=7` a destra
- **`y=0` in BASSO, `y=7` in ALTO** (combacia col server)

Sulla matrice fisica fate voi la conversione riga (spesso `riga = ALTEZZA-1 - y`).

---

## 6️⃣ Cosa fate voi / cosa NON fate

**Fate voi:**
- ✅ Driver matrice LED (FastLED/NeoPixel), colori, animazioni, mapping `(x,y)→LED`
- ✅ Lettura joystick (ADC, deadzone, debounce), pin GPIO (`pins.h` vostro)
- ✅ Il modello di piazzamento navi (riempite `boats[]`)
- ✅ La mira (scegliete `x,y` e chiamate `battle_shoot`)
- ✅ Il `.ino` principale (`setup`/`loop`) con le `battle_*`

**Non fate:**
- ❌ Niente `WiFi.begin`, client MQTT, JSON
- ❌ Non scrivete su `g_state` (lo aggiorniamo noi) — solo lettura
- ❌ Non toccate `battle.*`, `net_*`, `protocol.*`, `game_state.*`

---

## 7️⃣ Test in isolamento (senza server)

Per provare display/input senza rete, potete forzare a mano le board e non
chiamare le `battle_*`:
```cpp
void setup() {
  interface_hw_init();
  g_state.my_role = Role::Host;
  g_state.own_board[3][3] = OwnCell::Ship;
  g_state.enemy_board[2][2] = EnemyCell::Hit;
}
void loop() { interface_redraw(); }   // disegnate quello che leggete
```

Per il test col server vero, vedi `ARCHITECTURE.md` — c'è uno snippet PowerShell
che simula un secondo giocatore via `mosquitto_pub`.

---

## 8️⃣ FAQ

**Le `battle_*` bloccano davvero? Non si pianta la rete?**
Bloccano il *vostro* loop, non la rete: WiFi/MQTT girano su un core separato e
restano vivi. Quando il server risponde, la funzione ritorna.

**Come so se ho vinto o perso?**
Dopo che `battle_over()` è true: `battle_winner() == g_state.my_role` → vittoria,
altrimenti sconfitta.

**Quale board mostro?**
- Mio turno: `enemy_board` (dove sparo)
- Turno avversario: `own_board` (dove mi spara)

**Quante navi nel setup?**
Vedi `FLEET_LENS` in `game_config.h` (ora: due da 2, una da 3, una da 4 — come
la vostra `availableBoats`). Se volete una flotta diversa, si cambia lì insieme.

**Il cursore lo gestite voi?**
Sì: nel modello a facciata l'input è tutto vostro. Noi non tracciamo più un
cursore — voi scegliete le coordinate e ce le passate (`battle_shoot`, `boats[]`).

**Posso animare durante l'attesa dell'avversario?**
Nel modello lineare no (il loop è fermo dentro `battle_await_change`). Se serve,
lo facciamo con un task display separato — chiedete.

---

Se qualcosa non torna, ditecelo e lo chiariamo. Buon lavoro!

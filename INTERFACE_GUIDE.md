# Guida per il team Interface 👋

Tutto quello che vi serve per collegare il vostro codice (joystick + matrice LED)
al nostro (rete + MQTT + logica di gioco). Documento corto apposta: 5 minuti
di lettura e potete cominciare.

---

## TL;DR

1. **Quando il joystick si muove o si clicca**, chiamate `app_on_input(InputEvent)`
2. **Quando dovete disegnare la matrice**, leggete la variabile globale `g_state`
3. **Non vi serve sapere niente** di WiFi, MQTT, JSON, server. Quello lo facciamo noi sotto.

---

## Dove lavorare

```bash
git clone https://github.com/leooon3/Battleship-Project.git
cd Battleship-Project
git checkout interface
```

Tutto il codice (vostro e nostro) vive nella cartella `Battleship_ESP32/` come
unico sketch Arduino. Voi aggiungete file nuovi (es. `display.cpp`,
`joystick.cpp`, `pins.h`), noi non li tocchiamo.

---

## 1️⃣ Cosa chiamate VOI quando il giocatore agisce

📂 File: `Battleship_ESP32/hal.h`

```cpp
enum class InputEvent : uint8_t {
  Up,        // joystick verso l'alto   (cursore y+1)
  Down,      // joystick verso il basso (cursore y-1)
  Left,      // joystick a sinistra     (cursore x-1)
  Right,     // joystick a destra       (cursore x+1)
  BtnShort,  // click corto del pulsante
  BtnLong,   // click lungo del pulsante
};

void app_on_input(InputEvent e);
```

Esempio d'uso nel vostro codice:

```cpp
#include "hal.h"

void joystick_poll() {
  // ... il vostro debouncing, deadzone, repeat-rate ecc.
  if (joystick_just_moved_up())    app_on_input(InputEvent::Up);
  if (joystick_just_moved_down())  app_on_input(InputEvent::Down);
  if (button_just_short_pressed()) app_on_input(InputEvent::BtnShort);
  if (button_just_long_pressed())  app_on_input(InputEvent::BtnLong);
}
```

### Note importanti

- **Eventi discreti**: una chiamata = uno step. Se il giocatore tiene il joystick
  in alto per 1 secondo e voi volete che il cursore si muova 5 volte, chiamate
  `app_on_input(Up)` 5 volte (con il vostro repeat-rate).
- **Sempre sicuro**: potete chiamarla anche da ISR. Noi accodiamo l'evento e lo
  processiamo nel loop principale.
- **Non vi serve sapere in che fase è il gioco**: il significato dell'input
  cambia in base alla fase (in `SettingUp` un click corto ruota la nave, in
  `Playing` un click corto spara), ma la FSM ci pensa lei. Voi mandate sempre
  l'evento grezzo.

---

## 2️⃣ Cosa leggete VOI per disegnare

📂 File: `Battleship_ESP32/game_state.h`

```cpp
extern GameState g_state;   // variabile globale, includete game_state.h

struct GameState {
  Role     my_role;             // Host, Guest, o None (non ancora assegnato)
  uint32_t game_id;
  Role     turn;                // di chi è il turno

  OwnCell   own_board  [BOARD_W][BOARD_H];   // la VOSTRA flotta
  EnemyCell enemy_board[BOARD_W][BOARD_H];   // quello che sapete dell'avversario

  uint8_t cursor_x, cursor_y;   // posizione del cursore

  // Wizard piazzamento navi (fase SettingUp)
  uint8_t   setup_index;        // quale nave sto piazzando ora (0..3)
  Direction setup_dir;          // North/East/South/West
  Boat      placed_boats[4];    // navi già confermate

  // Contatori (utili per decidere "ho vinto / ho perso")
  uint8_t my_ships_sunk;        // 0..4. Se = 4 → ho perso
  uint8_t enemy_ships_sunk;     // 0..4. Se = 4 → ho vinto
};
```

### Cosa significa una cella

```cpp
enum class OwnCell {
  Empty,  // acqua vuota
  Ship,   // mia nave intatta
  Hit,    // mia nave colpita
  Sunk,   // mia nave affondata
  Miss,   // avversario ha sparato qui ma era acqua
};

enum class EnemyCell {
  Unknown,  // non ho ancora sparato qui
  Miss,     // ho sparato e ho mancato
  Hit,      // ho colpito (ma la nave non è affondata)
  Sunk,     // ho affondato la nave
};
```

### In che fase siamo? (per decidere cosa disegnare)

📂 File: `Battleship_ESP32/app_fsm.h`

```cpp
AppPhase app_fsm_phase();

enum class AppPhase {
  Init,             // boot, mai per voi
  WaitingNet,       // WiFi/MQTT in connessione
  Registering,      // attesa pairing col server
  SettingUp,        // wizard di piazzamento navi
  WaitingGameStart, // setup inviato, attesa che parta partita
  Playing,          // partita in corso
  End,              // partita finita
};
```

---

## 3️⃣ Schema mentale del vostro codice

```cpp
#include "hal.h"
#include "game_state.h"
#include "app_fsm.h"
// + le vostre lib (FastLED, ecc.)

void display_setup()  { /* FastLED.addLeds... */ }
void joystick_setup() { /* pinMode, ecc. */ }

void joystick_poll() {
  // leggete ADC, debouncing, edge detection
  // → chiamate app_on_input(...) sui fronti
}

void display_render() {
  switch (app_fsm_phase()) {
    case AppPhase::SettingUp:
      // disegnate own_board + anteprima nave corrente (setup_index, setup_dir)
      // + cursore lampeggiante a (cursor_x, cursor_y)
      break;

    case AppPhase::Playing:
      if (g_state.turn == g_state.my_role) {
        // mio turno: disegnate enemy_board + cursore di mira
      } else {
        // turno avversario: disegnate own_board (vedete i colpi che subite)
      }
      break;

    case AppPhase::End:
      if (g_state.enemy_ships_sunk >= 4) draw_victory();
      else                                draw_defeat();
      break;

    default:
      // Init, WaitingNet, Registering, WaitingGameStart
      draw_connecting_animation();
      break;
  }
}
```

Nel `Battleship_ESP32.ino` chiamate il vostro `display_setup()`/`joystick_setup()`
nel `setup()` esistente, e `joystick_poll()`/`display_render()` nel `loop()`.

---

## 4️⃣ Coordinate

- Board 8×8: `x ∈ [0..7]`, `y ∈ [0..7]`
- `x=0` è a sinistra, `x=7` a destra
- **`y=0` è in BASSO, `y=7` è in ALTO** (combacia col server Rust)
- Joystick "Up" → cursore `y+1`

Quando piazzate i LED fisicamente, ricordatevi questa convenzione (o specchiate
nel mapping `(x, y) → indice LED`).

---

## 5️⃣ Cosa NON fate

- ❌ Niente `WiFi.begin`, niente client MQTT, niente JSON
- ❌ Non scrivete su `g_state` (lo aggiorniamo noi quando il server manda eventi)
- ❌ Niente `Serial.begin(115200)` nel `setup()` (l'abbiamo già fatto noi)
- ❌ Non toccate i file in `net_*`, `proto_*`, `app_fsm*`, `game_state*`

## 6️⃣ Cosa fate VOI

- ✅ Driver matrice LED (FastLED, NeoPixel, qualunque)
- ✅ Lettura ADC joystick, deadzone, edge detection, debouncing, repeat-rate
- ✅ Soglia click corto vs lungo (consigliato ~600ms)
- ✅ Scelta dei pin GPIO (mettete un `pins.h` vostro)
- ✅ Brightness, color palette, animazioni
- ✅ Mapping fisico `(x, y) → indice LED` della matrice
- ✅ Le vostre `setup()` e `loop()` chiamate dal main `.ino`

---

## 6.5 ️⃣ Test in isolamento (senza server)

Se volete provare il vostro codice display/input senza far partire server Rust +
Mosquitto, potete:

**Forzare uno stato a mano** nel vostro `setup()`:
```cpp
g_state.my_role = Role::Host;
g_state.turn    = Role::Host;
g_state.own_board[3][3] = OwnCell::Ship;
g_state.cursor_x = 4;
g_state.cursor_y = 4;
// poi il vostro display_render() disegnerà di conseguenza
```

**Iniettare input fake**:
```cpp
delay(1000);
app_on_input(InputEvent::Up);     // simula joystick in alto
delay(500);
app_on_input(InputEvent::BtnShort);
```

Per il test col server vero (ESP32 connesso al broker), vedi `ARCHITECTURE.md`
sezione 9 — c'è uno snippet PowerShell che simula un secondo giocatore.

---

## 7️⃣ FAQ veloci

**Come ricevo notifiche quando lo stato cambia?**
Non c'è callback. Disegnate ogni ~30 fps leggendo `g_state`. Se è cambiato, lo
vedete al prossimo frame.

**Cosa succede se chiamo `app_on_input` in una fase dove non ha senso?**
Viene accodato. La FSM lo processa o lo ignora in base alla fase. Non fa danni
mai, potete chiamarla sempre.

**Come faccio a sapere se la partita è vinta o persa?**
- `app_fsm_phase() == AppPhase::End`
- Se `g_state.enemy_ships_sunk >= 4` → vittoria
- Se `g_state.my_ships_sunk >= 4` → sconfitta

**Quale board mostro durante il mio turno?**
- Mio turno (`turn == my_role`): mostra `enemy_board` (sparo lì)
- Turno avversario: mostra `own_board` (vedo dove mi spara)

**Quante navi devo aspettarmi nel setup wizard?**
4 navi totali: una `len=2` e tre `len=1`. La sequenza è in `FLEET_LENS` in
`game_config.h`. Il giocatore le piazza una alla volta tramite il wizard
(`Up/Down/Left/Right` muove cursore, `BtnShort` ruota, `BtnLong` conferma).

**Posso modificare `Battleship_ESP32.ino`?**
Sì. Per inserire le vostre `setup`/`loop` di hardware. Coordinatevi con noi via
PR per non finire in conflitti di merge.

---

Se qualcosa non torna o c'è un'ambiguità in questo doc → ditecelo, lo chiariamo
subito. Buon lavoro!

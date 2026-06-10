// ============================================================================
// app_fsm.cpp
//
// Macchina a stati principale. Reagisce a:
//   - eventi MQTT (assign / state / event)
//   - eventi input (app_on_input dal modulo joystick via hal.h)
//
// Espone solo lo stato logico (g_state e fase). Il renderer guarda lo stato
// e disegna; la rete pubblica quando la FSM lo decide.
// ============================================================================
#include "app_fsm.h"
#include "game_config.h"
#include "game_state.h"
#include "net_mqtt.h"
#include "net_wifi.h"
#include "proto_types.h"
#include "hal.h"

// ---- Stato FSM ----
static AppPhase  s_phase       = AppPhase::Init;

// ---- Coda input lock-free (single producer single consumer) ----
// Il team joystick puo' chiamare app_on_input() anche da ISR: scriviamo
// solo il tail, leggiamo solo il head nel loop. Niente race su questi due
// scalari su una singola CPU se restano volatile.
static constexpr uint8_t INPUT_Q_SIZE = 16;
static volatile InputEvent s_in_queue[INPUT_Q_SIZE];
static volatile uint8_t    s_in_head = 0;
static volatile uint8_t    s_in_tail = 0;

static bool input_pop(InputEvent& out) {
  if (s_in_head == s_in_tail) return false;
  out = s_in_queue[s_in_head];
  s_in_head = (s_in_head + 1) % INPUT_Q_SIZE;
  return true;
}

// ---- Callback MQTT ----
static void on_mqtt_connected();
static void on_assign(const proto::AssignMsg& m);
static void on_state(const proto::StateMsg& m);
static void on_event(const proto::EventMsg& m);

// ---- Handler input per fase ----
static void handle_input_setup(InputEvent e);
static void handle_input_playing(InputEvent e);

// ---- Helpers transizioni ----
static void enter_setup();
static void publish_setup_and_wait();

// ============================================================================
// API
// ============================================================================

void app_fsm_begin() {
  game_state_reset();
  s_phase = AppPhase::WaitingNet;

  net_mqtt_set_on_connected(on_mqtt_connected);
  net_mqtt_set_on_assign   (on_assign);
  net_mqtt_set_on_state    (on_state);
  net_mqtt_set_on_event    (on_event);

  Serial.println("[fsm] WaitingNet");
}

void app_fsm_loop() {
  // 1) Consuma eventi input.
  InputEvent e;
  while (input_pop(e)) {
    switch (s_phase) {
      case AppPhase::SettingUp: handle_input_setup(e);   break;
      case AppPhase::Playing:   handle_input_playing(e); break;
      default:                  /* ignora input fuori contesto */ break;
    }
  }

  // (Niente log periodico: tutto cio' che e' interessante e' gia' loggato
  // event-driven sotto, sulle transizioni di fase e callback MQTT.)
}

AppPhase app_fsm_phase() { return s_phase; }

const char* app_fsm_phase_str() {
  switch (s_phase) {
    case AppPhase::Init:             return "Init";
    case AppPhase::WaitingNet:       return "WaitingNet";
    case AppPhase::Registering:      return "Registering";
    case AppPhase::SettingUp:        return "SettingUp";
    case AppPhase::WaitingGameStart: return "WaitingGameStart";
    case AppPhase::Playing:          return "Playing";
    case AppPhase::End:              return "End";
  }
  return "?";
}

// ============================================================================
// Implementazione di app_on_input (dichiarata in hal.h)
// ============================================================================
//
// Chiamata dal team joystick. Push in coda, niente logica qui per restare
// ISR-safe. Drop silente se la coda e' piena.
//
void app_on_input(InputEvent e) {
  uint8_t next = (s_in_tail + 1) % INPUT_Q_SIZE;
  if (next == s_in_head) return; // queue full -> drop
  s_in_queue[s_in_tail] = e;
  s_in_tail = next;
}

// ============================================================================
// Callback MQTT
// ============================================================================

static void on_mqtt_connected() {
  const char* mac = net_wifi_mac();
  Serial.println("[fsm] MQTT up -> sub assign, pub register");
  net_mqtt_subscribe_assign(mac);
  net_mqtt_publish_register(mac);
  s_phase = AppPhase::Registering;
  Serial.println("[fsm] -> Registering");
}

static void on_assign(const proto::AssignMsg& m) {
  if (s_phase != AppPhase::Registering) {
    Serial.printf("[fsm] assign in phase=%s, accepting\n", app_fsm_phase_str());
  }
  g_state.my_role = m.role;
  g_state.game_id = m.game_id;
  Serial.printf("[fsm] assigned role=%s game_id=%lu\n",
                role_to_str(g_state.my_role),
                (unsigned long)g_state.game_id);

  net_mqtt_subscribe_game(g_state.game_id, g_state.my_role);
  enter_setup();
}

static void on_state(const proto::StateMsg& m) {
  g_state.turn = m.turn;
  switch (s_phase) {
    case AppPhase::Playing:
      // Normale aggiornamento del turno.
      Serial.printf("[fsm] turn -> %s\n", role_to_str(g_state.turn));
      break;
    case AppPhase::End:
      // Partita finita per la nostra deduzione, ma il server continua ad
      // alternare i turni: ignoriamo i state successivi.
      break;
    default:
      // Primo state dopo una fase pre-gioco (WaitingGameStart, SettingUp,
      // Registering, WaitingNet, Init): la partita e' iniziata.
      s_phase = AppPhase::Playing;
      Serial.printf("[fsm] game start! first turn=%s -> Playing\n",
                    role_to_str(g_state.turn));
      break;
  }
}

static void on_event(const proto::EventMsg& m) {
  Serial.printf("[fsm] event: attacker=%s hit=%s at (%u,%u)\n",
                role_to_str(m.attacker),
                hitresult_to_str(m.hit),
                m.x, m.y);
  game_state_apply_event(m);

  // Deduzione game over dai contatori. Il server non manda un evento
  // esplicito di fine partita: lo deduciamo noi quando una flotta intera
  // viene affondata. Cosi' il renderer puo' leggere app_fsm_phase()==End
  // per disegnare vittoria/sconfitta.
  if (s_phase == AppPhase::Playing) {
    if (g_state.enemy_ships_sunk >= FLEET_COUNT) {
      s_phase = AppPhase::End;
      Serial.println("[fsm] WIN! enemy fleet destroyed -> End");
    } else if (g_state.my_ships_sunk >= FLEET_COUNT) {
      s_phase = AppPhase::End;
      Serial.println("[fsm] LOSS! own fleet destroyed -> End");
    }
  }
}

// ============================================================================
// Transizioni
// ============================================================================

static void enter_setup() {
  // Inizializza cursore al centro, direzione di default East, indice 0.
  g_state.cursor_x    = BOARD_W / 2;
  g_state.cursor_y    = BOARD_H / 2;
  g_state.setup_index = 0;
  g_state.setup_dir   = Direction::East;
  s_phase = AppPhase::SettingUp;
  Serial.printf("[fsm] -> SettingUp (need to place %d boats)\n", FLEET_COUNT);
}

static void publish_setup_and_wait() {
  Boat boats[FLEET_COUNT];
  size_t n = game_state_setup_get_boats(boats, FLEET_COUNT);
  Serial.printf("[fsm] publishing setup with %u boats\n", (unsigned)n);
  net_mqtt_publish_setup(g_state.game_id, g_state.my_role, boats, n);
  s_phase = AppPhase::WaitingGameStart;
  Serial.println("[fsm] -> WaitingGameStart");
}

// ============================================================================
// Handler input
// ============================================================================

static void handle_input_setup(InputEvent e) {
  switch (e) {
    case InputEvent::Up:    game_state_cursor_move( 0, +1); break;
    case InputEvent::Down:  game_state_cursor_move( 0, -1); break;
    case InputEvent::Left:  game_state_cursor_move(-1,  0); break;
    case InputEvent::Right: game_state_cursor_move(+1,  0); break;

    case InputEvent::BtnShort:
      // Ruota nave corrente
      game_state_setup_rotate();
      Serial.printf("[fsm] setup rotate -> %s\n",
                    direction_to_str(g_state.setup_dir));
      break;

    case InputEvent::BtnLong:
      // Conferma piazzamento
      if (game_state_setup_try_confirm()) {
        Serial.printf("[fsm] setup placed boat %u/%u (len=%u dir=%s at %u,%u)\n",
                      g_state.setup_index, (unsigned)FLEET_COUNT,
                      (unsigned)g_state.placed_boats[g_state.setup_index - 1].len,
                      direction_to_str(g_state.placed_boats[g_state.setup_index - 1].dir),
                      g_state.placed_boats[g_state.setup_index - 1].x,
                      g_state.placed_boats[g_state.setup_index - 1].y);

        if (game_state_setup_complete()) {
          publish_setup_and_wait();
        }
      } else {
        Serial.println("[fsm] setup invalid placement, ignored");
      }
      break;
  }
}

static void handle_input_playing(InputEvent e) {
  if (g_state.turn != g_state.my_role) {
    // Non e' il mio turno: ignoro tutto.
    return;
  }

  switch (e) {
    case InputEvent::Up:    game_state_cursor_move( 0, +1); break;
    case InputEvent::Down:  game_state_cursor_move( 0, -1); break;
    case InputEvent::Left:  game_state_cursor_move(-1,  0); break;
    case InputEvent::Right: game_state_cursor_move(+1,  0); break;

    case InputEvent::BtnShort:
    case InputEvent::BtnLong:
      // Spara alla cella corrente, se non gia' sparata.
      if (!game_state_aim_can_shoot()) {
        Serial.printf("[fsm] cell (%u,%u) gia' sparata, ignoro\n",
                      g_state.cursor_x, g_state.cursor_y);
        break;
      }
      Serial.printf("[fsm] shoot at (%u,%u)\n",
                    g_state.cursor_x, g_state.cursor_y);
      net_mqtt_publish_shoot(g_state.game_id, g_state.my_role,
                             g_state.cursor_x, g_state.cursor_y);
      // Nota: NON aggiorniamo enemy_board qui. Aspettiamo l'event dal server,
      // che ce lo confermera' (e' la fonte di verita').
      break;
  }
}

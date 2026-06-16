#include "app_fsm.h"
#include "game_config.h"
#include "game_state.h"
#include "net_mqtt.h"
#include "net_wifi.h"
#include "hal.h"

static AppPhase s_phase = AppPhase::Init;

// Lock-free single-producer/single-consumer queue: app_on_input() (possibly
// from an ISR) writes the tail, the loop reads the head.
static constexpr uint8_t   INPUT_Q_SIZE = 16;
static volatile InputEvent s_queue[INPUT_Q_SIZE];
static volatile uint8_t    s_head = 0;
static volatile uint8_t    s_tail = 0;

static bool input_pop(InputEvent& out) {
  if (s_head == s_tail) return false;
  out = s_queue[s_head];
  s_head = (s_head + 1) % INPUT_Q_SIZE;
  return true;
}

void app_on_input(InputEvent e) {
  uint8_t next = (s_tail + 1) % INPUT_Q_SIZE;
  if (next == s_head) return;  // full, drop
  s_queue[s_tail] = e;
  s_tail = next;
}

static void handle_input_ready(InputEvent e);
static void handle_input_setup(InputEvent e);
static void handle_input_playing(InputEvent e);

static void on_mqtt_connected() {
  // Subscribe to our reply topic now, but don't register yet: the display
  // shows an idle "waiting" screen and we register only when the player
  // presses the button (see handle_input_ready).
  net_mqtt_subscribe_assign(net_wifi_mac());
  s_phase = AppPhase::Ready;
  Serial.println("[fsm] -> Ready (press button to start)");
}

static void start_registering() {
  net_mqtt_publish_register(net_wifi_mac());
  s_phase = AppPhase::Registering;
  Serial.println("[fsm] -> Registering");
}

static void on_assign(const proto::AssignMsg& m) {
  g_state.my_role = m.role;
  g_state.game_id = m.game_id;
  Serial.printf("[fsm] assigned role=%s game_id=%lu\n",
                role_to_str(m.role), (unsigned long)m.game_id);

  net_mqtt_subscribe_game(m.game_id, m.role);

  g_state.cursor_x    = BOARD_W / 2;
  g_state.cursor_y    = BOARD_H / 2;
  g_state.setup_index = 0;
  g_state.setup_dir   = Direction::East;
  s_phase = AppPhase::SettingUp;
  Serial.printf("[fsm] -> SettingUp (%d boats to place)\n", FLEET_COUNT);
}

static void on_state(const proto::StateMsg& m) {
  if (m.over) {
    g_state.winner = m.winner;
    s_phase = AppPhase::End;
    Serial.printf("[fsm] game over, winner=%s -> End\n", role_to_str(m.winner));
    return;
  }

  g_state.turn = m.turn;
  if (s_phase == AppPhase::Playing) {
    Serial.printf("[fsm] turn -> %s\n", role_to_str(m.turn));
  } else if (s_phase != AppPhase::End) {
    // First state after setup means the game has started.
    s_phase = AppPhase::Playing;
    Serial.printf("[fsm] game start, first turn=%s -> Playing\n", role_to_str(m.turn));
  }
}

static void on_event(const proto::EventMsg& m) {
  Serial.printf("[fsm] event: attacker=%s hit=%s at (%u,%u)\n",
                role_to_str(m.attacker), hitresult_to_str(m.hit), m.x, m.y);
  game_state_apply_event(m);
}

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
  InputEvent e;
  while (input_pop(e)) {
    switch (s_phase) {
      case AppPhase::Ready:     handle_input_ready(e);   break;
      case AppPhase::SettingUp: handle_input_setup(e);   break;
      case AppPhase::Playing:   handle_input_playing(e); break;
      default: break;
    }
  }
}

AppPhase app_fsm_phase() { return s_phase; }

const char* app_fsm_phase_str() {
  switch (s_phase) {
    case AppPhase::Init:             return "Init";
    case AppPhase::WaitingNet:       return "WaitingNet";
    case AppPhase::Ready:            return "Ready";
    case AppPhase::Registering:      return "Registering";
    case AppPhase::SettingUp:        return "SettingUp";
    case AppPhase::WaitingGameStart: return "WaitingGameStart";
    case AppPhase::Playing:          return "Playing";
    case AppPhase::End:              return "End";
  }
  return "?";
}

// Idle screen after connecting: any button press kicks off registration.
static void handle_input_ready(InputEvent e) {
  if (e == InputEvent::BtnShort || e == InputEvent::BtnLong) {
    start_registering();
  }
}

static void handle_input_setup(InputEvent e) {
  switch (e) {
    case InputEvent::Up:    game_state_cursor_move( 0, +1); break;
    case InputEvent::Down:  game_state_cursor_move( 0, -1); break;
    case InputEvent::Left:  game_state_cursor_move(-1,  0); break;
    case InputEvent::Right: game_state_cursor_move(+1,  0); break;

    case InputEvent::BtnShort:
      game_state_setup_rotate();
      break;

    case InputEvent::BtnLong:
      if (!game_state_setup_try_confirm()) {
        Serial.println("[fsm] invalid placement");
        break;
      }
      Serial.printf("[fsm] placed boat %u/%d\n", g_state.setup_index, FLEET_COUNT);
      if (game_state_setup_complete()) {
        Boat boats[FLEET_COUNT];
        size_t n = game_state_setup_get_boats(boats, FLEET_COUNT);
        net_mqtt_publish_setup(g_state.game_id, g_state.my_role, boats, n);
        s_phase = AppPhase::WaitingGameStart;
        Serial.println("[fsm] -> WaitingGameStart");
      }
      break;
  }
}

static void handle_input_playing(InputEvent e) {
  if (g_state.turn != g_state.my_role) return;  // not my turn

  switch (e) {
    case InputEvent::Up:    game_state_cursor_move( 0, +1); break;
    case InputEvent::Down:  game_state_cursor_move( 0, -1); break;
    case InputEvent::Left:  game_state_cursor_move(-1,  0); break;
    case InputEvent::Right: game_state_cursor_move(+1,  0); break;

    case InputEvent::BtnShort:
    case InputEvent::BtnLong:
      if (!game_state_aim_can_shoot()) break;  // already fired here
      // Don't update enemy_board yet: the server's event is the source of truth.
      net_mqtt_publish_shoot(g_state.game_id, g_state.my_role,
                             g_state.cursor_x, g_state.cursor_y);
      break;
  }
}

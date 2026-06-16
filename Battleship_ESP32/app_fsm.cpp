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

static void handle_input_setup(InputEvent e);
static void handle_input_playing(InputEvent e);

static void on_mqtt_connected() {
  const char* mac = net_wifi_mac();
  net_mqtt_subscribe_assign(mac);
  net_mqtt_publish_register(mac);
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
  g_state.turn = m.turn;
  switch (s_phase) {
    case AppPhase::Playing:
      Serial.printf("[fsm] turn -> %s\n", role_to_str(m.turn));
      break;
    case AppPhase::End:
      break;  // game's over for us; server keeps alternating turns
    default:
      // First state after setup means the game has started.
      s_phase = AppPhase::Playing;
      Serial.printf("[fsm] game start, first turn=%s -> Playing\n", role_to_str(m.turn));
      break;
  }
}

static void on_event(const proto::EventMsg& m) {
  Serial.printf("[fsm] event: attacker=%s hit=%s at (%u,%u)\n",
                role_to_str(m.attacker), hitresult_to_str(m.hit), m.x, m.y);
  game_state_apply_event(m);

  // The server sends no game-over message, so deduce it from the sunk counts.
  if (s_phase == AppPhase::Playing) {
    if (g_state.enemy_ships_sunk >= FLEET_COUNT) {
      s_phase = AppPhase::End;
      Serial.println("[fsm] WIN -> End");
    } else if (g_state.my_ships_sunk >= FLEET_COUNT) {
      s_phase = AppPhase::End;
      Serial.println("[fsm] LOSS -> End");
    }
  }
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
    if (s_phase == AppPhase::SettingUp) handle_input_setup(e);
    else if (s_phase == AppPhase::Playing) handle_input_playing(e);
  }
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

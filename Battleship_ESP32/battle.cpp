#include "battle.h"
#include "app_fsm.h"
#include "secrets.h"
#include "net_wifi.h"
#include "net_mqtt.h"
#include "game_state.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Networks to try, highest priority first
static const WifiNet NETS[] = {
  { WIFI_PI_SSID, WIFI_PI_PASS, WIFI_PI_MQTT, WIFI_PI_IP, WIFI_PI_GW },
  { WIFI_FB_SSID, WIFI_FB_PASS, WIFI_FB_MQTT, WIFI_FB_IP, WIFI_FB_GW },
};

// Written by the MQTT callbacks and polled by the battle_* functions
static volatile bool      s_mqtt_up     = false;
static volatile bool      s_assigned    = false;
static volatile bool      s_started     = false;
static volatile bool      s_over        = false;
static volatile bool      s_my_turn     = false;
static volatile Role      s_winner      = Role::None;
static volatile bool      s_shot_wait   = false;   // a shot is awaiting its result
static volatile bool      s_shot_ready  = false;
static volatile HitResult s_shot_result = HitResult::Water;
static volatile bool      s_opp_shot_ready  = false;
static volatile uint8_t   s_opp_shot_x      = 0;
static volatile uint8_t   s_opp_shot_y      = 0;
static volatile HitResult s_opp_shot_result = HitResult::Water;
static volatile bool      s_state_ready     = false;
static bool               s_mqtt_started = false;

// Game phase, exposed to the display
static volatile AppPhase  s_phase = AppPhase::Init;

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

// --- MQTT callbacks ---

static void on_connected() {
  // Subscribe to reply topic before anyone publishes a register.
  net_mqtt_subscribe_assign(net_wifi_mac());
  s_phase = AppPhase::Ready;
  s_mqtt_up = true;
}

static void on_assign(const proto::AssignMsg& m) {
  g_state.my_role = m.role;
  g_state.game_id = m.game_id;
  net_mqtt_subscribe_game(m.game_id, m.role);
  s_phase = AppPhase::SettingUp;
  s_assigned = true;
}

static void on_state(const proto::StateMsg& m) {
  if (m.over) {
    s_winner = m.winner;
    s_phase = AppPhase::End;
    s_over = true;
    s_state_ready = true;
    return;
  }
  s_my_turn = (m.turn == g_state.my_role);
  s_phase = AppPhase::Playing;
  s_started = true;
  s_state_ready = true;
}

static void on_event(const proto::EventMsg& m) {
  game_state_apply_event(m);
  if (m.attacker == g_state.my_role && s_shot_wait) {
    s_shot_result = m.hit;
    s_shot_wait = false;
    s_shot_ready = true;
  } else if (m.attacker != g_state.my_role) {
    s_opp_shot_x = m.x;
    s_opp_shot_y = m.y;
    s_opp_shot_result = m.hit;
    s_opp_shot_ready = true;
  }
}

// --- network task: keeps WiFi/MQTT connected and reconnecting ---

static void net_task(void*) {
  net_wifi_begin(NETS, sizeof(NETS) / sizeof(NETS[0]));
  for (;;) {
    net_wifi_loop();
    if (net_wifi_is_connected() && !s_mqtt_started) {
#ifdef MQTT_USER
      net_mqtt_begin(net_wifi_mqtt_host(), MQTT_PORT, MQTT_USER, MQTT_PASS, net_wifi_mac());
#else
      net_mqtt_begin(net_wifi_mqtt_host(), MQTT_PORT, nullptr, nullptr, net_wifi_mac());
#endif
      s_mqtt_started = true;
    }
    if (s_mqtt_started) net_mqtt_loop();
    vTaskDelay(pdMS_TO_TICKS(100));
  }
}

static void wait_until(volatile bool& flag) {
  while (!flag) vTaskDelay(pdMS_TO_TICKS(5));
}

// --- facade ---

void battle_begin() {
  game_state_reset();
  s_phase = AppPhase::WaitingNet;
  net_mqtt_set_on_connected(on_connected);
  net_mqtt_set_on_assign(on_assign);
  net_mqtt_set_on_state(on_state);
  net_mqtt_set_on_event(on_event);

  xTaskCreatePinnedToCore(net_task, "net", 8192, nullptr, 1, nullptr, 0);
  wait_until(s_mqtt_up);
}

Role battle_register() {
  s_assigned = false;
  s_phase = AppPhase::Registering;
  net_mqtt_publish_register(net_wifi_mac());
  wait_until(s_assigned);
  return g_state.my_role;
}

void battle_send_setup(const Boat* boats, size_t n) {
  for (size_t i = 0; i < n; i++) game_state_mark_ship(boats[i]);
  s_started = false;
  s_phase = AppPhase::WaitingGameStart;
  net_mqtt_publish_setup(g_state.game_id, g_state.my_role, boats, n);
}

bool battle_game_started() { return s_started || s_over; }

bool battle_my_turn() { return s_my_turn; }
bool battle_over()    { return s_over; }
Role battle_winner()  { return s_winner; }

HitResult battle_shoot(uint8_t x, uint8_t y) {
  s_shot_ready = false;
  s_state_ready = false;
  s_shot_wait = true;
  net_mqtt_publish_shoot(g_state.game_id, g_state.my_role, x, y);
  wait_until(s_shot_ready);
  wait_until(s_state_ready);
  return s_shot_result;
}

void battle_await_change() {
  while (!s_my_turn && !s_over) vTaskDelay(pdMS_TO_TICKS(5));
}

HitResult battle_wait_for_opponent_shot(uint8_t& out_x, uint8_t& out_y) {
  s_opp_shot_ready = false;
  s_state_ready = false;
  while (!s_opp_shot_ready && !s_over) vTaskDelay(pdMS_TO_TICKS(5));
  if (s_over && !s_opp_shot_ready) {
    // Game ended without an event => Return safe defaults.
    out_x = 0;
    out_y = 0;
    return HitResult::Water;
  }
  wait_until(s_state_ready);
  out_x = s_opp_shot_x;
  out_y = s_opp_shot_y;
  return s_opp_shot_result;
}


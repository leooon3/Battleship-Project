// TEST driver for the synchronous facade (battle.h). Plays a full game in a
// linear, blocking style, reading shots from the Serial Monitor, so the facade
// can be exercised without joystick/matrix hardware.
//
// In production this file is replaced by the interface team's sketch: their
// display + joystick code, calling the same battle_* functions in the same
// linear way.
//
// Libraries: espMqttClient (Bert Melis), AsyncTCP (ESP32Async), ArduinoJson v7.
// Fill in secrets.h, flash, open Serial Monitor at 115200.

#include "battle.h"
#include "game_state.h"
#include "game_config.h"
#include <stdio.h>

// Hardcoded fleet for the test: 4-cell, 3-cell and two 2-cell ships.
// Boat layout is { len, dir, {x, y} }.
static const Boat TEST_FLEET[FLEET_COUNT] = {
  { 4, EAST, {0, 0} },   // (0,0)-(3,0)
  { 3, EAST, {0, 2} },   // (0,2)-(2,2)
  { 2, EAST, {0, 4} },   // (0,4)-(1,4)
  { 2, EAST, {0, 6} },   // (0,6)-(1,6)
};

static char own_char(OwnCell c) {
  switch (c) {
    case OwnCell::Ship: return 'S';
    case OwnCell::Hit:  return 'X';
    case OwnCell::Sunk: return '#';
    case OwnCell::Miss: return 'o';
    default:            return '.';
  }
}

static char enemy_char(EnemyCell c) {
  switch (c) {
    case EnemyCell::Miss: return 'o';
    case EnemyCell::Hit:  return 'X';
    case EnemyCell::Sunk: return '#';
    default:              return '.';
  }
}

static void print_boards() {
  Serial.println("   MY FLEET            ENEMY");
  for (int y = BOARD_H - 1; y >= 0; y--) {
    Serial.print(y); Serial.print("  ");
    for (int x = 0; x < BOARD_W; x++) { Serial.print(own_char(g_state.own_board[x][y])); Serial.print(' '); }
    Serial.print("   ");
    for (int x = 0; x < BOARD_W; x++) { Serial.print(enemy_char(g_state.enemy_board[x][y])); Serial.print(' '); }
    Serial.println();
  }
  Serial.println("   0 1 2 3 4 5 6 7      0 1 2 3 4 5 6 7");
}

// Block until a full line is typed; return it trimmed in 'out'.
static void read_line(char* out, size_t max) {
  size_t n = 0;
  for (;;) {
    if (!Serial.available()) { delay(5); continue; }
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (n == 0) continue;   // skip blank lines
      out[n] = 0;
      return;
    }
    if (n < max - 1) out[n++] = c;
  }
}

// Block until a valid "x y" shot is entered.
static void read_shot(uint8_t& x, uint8_t& y) {
  char line[24];
  for (;;) {
    read_line(line, sizeof(line));
    int xi, yi;
    if (sscanf(line, "%d %d", &xi, &yi) == 2 &&
        xi >= 0 && xi < BOARD_W && yi >= 0 && yi < BOARD_H) {
      x = (uint8_t)xi;
      y = (uint8_t)yi;
      return;
    }
    Serial.println("bad input; type: x y   (e.g. 3 5)");
  }
}

void setup() {
  Serial.begin(115200);
  delay(300);
  Serial.println("\n=== Battleship facade test ===");

  battle_begin();
  Serial.println("connected. Type anything + ENTER to start.");
  { char tmp[8]; read_line(tmp, sizeof(tmp)); }

  Role me = battle_register();
  Serial.printf("registered as %s\n", role_to_str(me));

  Serial.println("sending setup...");
  battle_send_setup(TEST_FLEET, FLEET_COUNT);
  Serial.println("game started!");

  while (!battle_over()) {
    print_boards();
    if (battle_my_turn()) {
      Serial.println(">> YOUR TURN. Type a shot: x y");
      uint8_t x, y;
      read_shot(x, y);
      HitResult r = battle_shoot(x, y);
      Serial.printf("shot (%u,%u) -> %s\n", x, y, hitresult_to_str(r));
    } else {
      Serial.println(".. opponent's turn, waiting ..");
      uint8_t ox, oy;
      HitResult or_ = battle_wait_for_opponent_shot(ox, oy);
      Serial.printf("opponent shot (%u,%u) -> %s\n", ox, oy, hitresult_to_str(or_));
    }
  } 

  print_boards();
  Serial.printf("GAME OVER - winner: %s\n", role_to_str(battle_winner()));
}

void loop() {
  delay(1000);  // the whole game runs in setup(); nothing to do here.
}

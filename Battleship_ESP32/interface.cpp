#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#include <map>
#include <vector>
#include "battle.h"
#include "game_state.h"
#include "app_fsm.h"

#define JOYSTICK_X 33
#define JOYSTICK_Y 34
#define JOYSTICK_BUTTON 14


#define MATRIX_WIDTH 8
#define MATRIX_HEIGHT 8
#define MATRIX_PIN 12

Adafruit_NeoMatrix matrix = Adafruit_NeoMatrix(
  MATRIX_WIDTH, MATRIX_HEIGHT, MATRIX_PIN,
  NEO_MATRIX_TOP + NEO_MATRIX_RIGHT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG,
  NEO_GRB + NEO_KHZ800);

enum State {
  WAITING,
  SETUP,
  PLAYING,
  RESULT,
};

State current_state;
std::vector<Boat> setup_boat;
Role my_role;

int cursor_x = 0;
int cursor_y = 0;


int readJoystickAxis(uint8_t pin) {
  int raw_value = analogRead(pin);
  float value = (raw_value / 2000.0 - 1.0);
  if (value < -0.85) {
    return -1;
  } else if (value > 0.85) {
    return 1;
  } else {
    return 0;
  }
}



int longest_boat(std::map<int, int> availableBoats) {
  for (auto it = availableBoats.rbegin(); it != availableBoats.rend(); ++it) {
    if (it->second > 0) {
      return it->first;
    }
  }
  return -1;
}

bool boat_contains(int next_x, int next_y){
 
  for (auto &boat : setup_boat) {
    int x0 = boat.initial_pos[0], y0 = boat.initial_pos[1];
    switch (boat.dir) {
      case NORTH: if (next_x == x0 && next_y >= y0 && next_y <= (y0 + boat.len -1)) return true; break;
      case SOUTH: if (next_x == x0 && next_y <= y0 && next_y >= (y0 - boat.len +1)) return true; break;
      case EAST:  if (next_y == y0 && next_x >= x0 && next_x <= (x0 + boat.len -1)) return true; break;
      case WEST:  if (next_y == y0 && next_x <= x0 && next_x >= (x0 - boat.len +1)) return true; break;
    }
  }
  return false;
}

void set_starting_cursor_position(int &cursor_x, int &cursor_y) {
  for (int i = 0; i < 8; i++) {
    for (int j = 0; j <= i; j++) {
      cursor_x = j;
      cursor_y = i - j;

      if (!boat_contains(cursor_x, cursor_y))
        return;
    }
  }
}

bool illegal(int next_x, int next_y, int longest) {
  bool ret = false;
  if (next_x * next_y != 0 && (next_x != 0 || next_y != 0)) {
    ret = true;
  } else if (abs(next_x) >= longest || abs(next_y) >= longest) {
    ret = true;
  }


  if ((cursor_x + next_x) < 0 || (cursor_x + next_x) >= MATRIX_WIDTH) {
    ret = true;
  }

  if ((cursor_y + next_y) < 0 || (cursor_y + next_y) >= MATRIX_HEIGHT) {
    ret = true;
  }
 

  if(boat_contains((next_x + cursor_x), (next_y + cursor_y))){
    ret = true;
  }


  return ret;
}


void winning_animation() {
  while(true){
    long start_millis = millis();
    matrix.clear();
    matrix.drawPixel( 2, 1, matrix.Color(255, 255, 0));
    matrix.drawPixel( 3, 1, matrix.Color(255, 255, 0));
    matrix.drawPixel( 4, 1, matrix.Color(255, 255, 0));
    matrix.drawPixel( 5, 1, matrix.Color(255, 255, 0));
    matrix.drawPixel( 1, 2, matrix.Color(255, 255, 0));
    matrix.drawPixel( 6, 2, matrix.Color(255, 255, 0));
    matrix.drawPixel( 2, 5, matrix.Color(255, 255, 0));
    matrix.drawPixel( 2, 6, matrix.Color(255, 255, 0));
    matrix.drawPixel( 5, 5, matrix.Color(255, 255, 0));
    matrix.drawPixel( 5, 6, matrix.Color(255, 255, 0));

    matrix.show();
    while (millis() < start_millis + 1000) {
      delay(50);
      if(!digitalRead(JOYSTICK_BUTTON)){
        return;
      }
    }
    matrix.clear();
    matrix.drawPixel( 1, 2, matrix.Color(0, 255, 0));
    matrix.drawPixel( 2, 1, matrix.Color(0, 255, 0));
    matrix.drawPixel( 3, 2, matrix.Color(0, 255, 0));
    matrix.drawPixel( 4, 3, matrix.Color(0, 255, 0));
    matrix.drawPixel( 5, 4, matrix.Color(0, 255, 0));
    matrix.drawPixel( 6, 5, matrix.Color(0, 255, 0));
    matrix.show();
    while (millis() < start_millis + 2000) {
      delay(50);
      if(!digitalRead(JOYSTICK_BUTTON)){
        return;
      }
    }
  }
}

void losing_animation() {
  while(true){
    long start_millis = millis();
    matrix.clear();
    matrix.drawPixel( 1, 1, matrix.Color(255, 255, 0));
    matrix.drawPixel( 6, 1, matrix.Color(255, 255, 0));
    matrix.drawPixel( 2, 2, matrix.Color(255, 255, 0));
    matrix.drawPixel( 3, 2, matrix.Color(255, 255, 0));
    matrix.drawPixel( 4, 2, matrix.Color(255, 255, 0));
    matrix.drawPixel( 5, 2, matrix.Color(255, 255, 0));
    matrix.drawPixel( 2, 5, matrix.Color(255, 255, 0));
    matrix.drawPixel( 2, 6, matrix.Color(255, 255, 0));
    matrix.drawPixel( 5, 5, matrix.Color(255, 255, 0));
    matrix.drawPixel( 5, 6, matrix.Color(255, 255, 0));
    matrix.show();
    while (millis() < start_millis + 1000) {
      delay(50);
      if(!digitalRead(JOYSTICK_BUTTON)){
        return;
      }
    }
    matrix.clear();
    matrix.drawPixel( 1, 1, matrix.Color(255, 0, 0));
    matrix.drawPixel( 6, 1, matrix.Color(255, 0, 0));
    matrix.drawPixel( 2, 2, matrix.Color(255, 0, 0));
    matrix.drawPixel( 5, 2, matrix.Color(255, 0, 0));
    matrix.drawPixel( 3, 3, matrix.Color(255, 0, 0));
    matrix.drawPixel( 4, 3, matrix.Color(255, 0, 0));
    matrix.drawPixel( 3, 4, matrix.Color(255, 0, 0));
    matrix.drawPixel( 4, 4, matrix.Color(255, 0, 0));
    matrix.drawPixel( 2, 5, matrix.Color(255, 0, 0));
    matrix.drawPixel( 5, 5, matrix.Color(255, 0, 0));
    matrix.drawPixel( 1, 6, matrix.Color(255, 0, 0));
    matrix.drawPixel( 6, 6, matrix.Color(255, 0, 0));
    matrix.show();
    while (millis() < start_millis + 2000) {
      delay(50);
      if(!digitalRead(JOYSTICK_BUTTON)){
        return;
      }
    }
  }
}

void waiting_animation(){
  while (true){
    for (int i = MATRIX_HEIGHT-1; i >= 0; i--){
      for (int j = 0; j < MATRIX_WIDTH; j++){
        matrix.drawPixel(j, i, matrix.Color(0, 240, 150));
        matrix.show();
        delay(100);
        matrix.drawPixel(j, i, matrix.Color(0, 0, 0));
        matrix.show();

        if(battle_game_started()){
          return;
        }
      }

    }

  }
}

void waiting() {
  matrix.clear();


  int boats[] = { 2, 2, 3, 4 };
  uint16_t colors[] = { matrix.Color(0, 240, 150), matrix.Color(0, 0, 240), matrix.Color(250, 24, 150), matrix.Color(250, 200, 0) };
  for (int i = 0; i < 4; i++) {
    int boat = boats[i];
    for (int j = 0; j < boat; j++) {
      matrix.drawPixel(i + 2, j + 2, colors[i]);
    }
  }

  matrix.show();

  while (digitalRead(JOYSTICK_BUTTON)) {
    delay(100);
  }

  my_role = battle_register();
  current_state = SETUP;
}



void setup_state() {

  setup_boat.clear();
  std::map<int, int> availableBoats;
  availableBoats[2] = 2;
  availableBoats[3] = 1;
  availableBoats[4] = 1;

  matrix.clear();
  int longest;

  while ((longest = longest_boat(availableBoats)) > 0) {
    set_starting_cursor_position(cursor_x, cursor_y);  
  
    while (digitalRead(JOYSTICK_BUTTON)) {
      int x = readJoystickAxis(JOYSTICK_Y);
      int y = readJoystickAxis(JOYSTICK_X);

      int next_cursor_x = cursor_x + x;
      int next_cursor_y = cursor_y + y;

      if (next_cursor_x >= 0 && next_cursor_x < MATRIX_WIDTH && next_cursor_y >= 0 && next_cursor_y < MATRIX_HEIGHT && !boat_contains(next_cursor_x, next_cursor_y) ) {
          
        if (!boat_contains(cursor_x, cursor_y)){
          matrix.drawPixel(cursor_x, cursor_y, matrix.Color(0,0,0));
        }
        
        cursor_x = next_cursor_x;
        cursor_y = next_cursor_y;
        matrix.drawPixel(cursor_x, cursor_y, matrix.Color(255,255,255));
      }

      matrix.show();
      delay(100);
    }

    while (!digitalRead(JOYSTICK_BUTTON)) {
      delay(100);
    }

    int offset_x = 0;
    int offset_y = 0;
    while (digitalRead(JOYSTICK_BUTTON)) {

      int x = readJoystickAxis(JOYSTICK_Y);
      int y = readJoystickAxis(JOYSTICK_X);

      if (x == 0 && y == 0 || x * y != 0) {
        delay(100);
        continue;
      }

      int next_offset_x = offset_x + x;
      int next_offset_y = offset_y + y;

      if (illegal(next_offset_x, next_offset_y, longest)) {
        delay(100);
        continue;
      }

      if (next_offset_x * x + next_offset_y * y > 0) {
        matrix.drawPixel(cursor_x + next_offset_x, cursor_y + next_offset_y, matrix.Color(255, 255, 255));
      } else {
        matrix.drawPixel(cursor_x + offset_x, cursor_y + offset_y, matrix.Color(0, 0, 0));
      }

      offset_x = next_offset_x;
      offset_y = next_offset_y;
      matrix.show();
      delay(100);
    }

    while (!digitalRead(JOYSTICK_BUTTON)) {
      delay(100);
    }


    Direction dir;
    int length = 0;
    if (offset_x > 0) {
      dir = EAST;
      length = offset_x + 1;
    } else if (offset_x < 0) {
      dir = WEST;
      length = abs(offset_x) + 1;
    } else if (offset_y > 0) {
      dir = NORTH;
      length = offset_y + 1;
    } else {
      dir = SOUTH;
      length = abs(offset_y) + 1;
    }

    auto it = availableBoats.find(length);
    if (it != availableBoats.end() && it->second > 0) {
      it -> second -= 1;
    }else{
      delay(100);
      switch(dir){
        case NORTH:
          for(int i = cursor_y; i < (cursor_y+length); i++){
            matrix.drawPixel(cursor_x, i, matrix.Color(255, 0, 0));
          }
          matrix.show();
          delay(250);

          for(int i = cursor_y; i < (cursor_y+length); i++){
            matrix.drawPixel(cursor_x, i, matrix.Color(0, 0, 0));
          }
          break;
        
        case SOUTH:
          for(int i = cursor_y; i > (cursor_y-length); i--){
            matrix.drawPixel(cursor_x, i, matrix.Color(255, 0, 0));
          }
          matrix.show();
          delay(250);

          for(int i = cursor_y; i > (cursor_y-length); i--){
            matrix.drawPixel(cursor_x, i, matrix.Color(0, 0, 0));
          }
          break;

        case WEST:
          for(int i = cursor_x; i > (cursor_x-length); i--){
            matrix.drawPixel(i, cursor_y, matrix.Color(255, 0, 0));
          }
          matrix.show();
          delay(250);

          for(int i = cursor_x; i > (cursor_x-length); i--){
            matrix.drawPixel(i, cursor_y, matrix.Color(0, 0, 0));
          }
          break;

        case EAST:
         for(int i = cursor_x; i < (cursor_x+length); i++){
            matrix.drawPixel(i, cursor_y, matrix.Color(255, 0, 0));
          }
          matrix.show();
          delay(250);

          for(int i = cursor_x; i < (cursor_x+length); i++){
            matrix.drawPixel(i, cursor_y, matrix.Color(0, 0, 0));
          }
          break;
      }
      offset_x = 0;
      offset_y = 0;
      
      continue;
    }
    setup_boat.push_back({ length, dir, { cursor_x, cursor_y } });
    
  }

  battle_send_setup(setup_boat.data(), setup_boat.size());

  matrix.clear();
  matrix.show();
  
  waiting_animation();

  current_state = PLAYING;

}

void playing_state(){
  
  if( battle_my_turn() ){
    cursor_x = 0;
    cursor_y = 0;
    matrix.clear();
    for (int i = 0; i < MATRIX_HEIGHT; i++){
      for(int j = 0; j < MATRIX_WIDTH; j++){
        switch (g_state.enemy_board[j][i]){
          case EnemyCell::Unknown:
            matrix.drawPixel(j, i, matrix.Color(0, 0, 0));
            break;

          case EnemyCell::Hit:
            matrix.drawPixel(j, i, matrix.Color(255, 255, 0));
            break;

          case EnemyCell::Miss:
            matrix.drawPixel(j, i, matrix.Color(0, 0, 255));
            break;

          case EnemyCell::Sunk:
            matrix.drawPixel(j, i, matrix.Color(255, 0, 0));
            break;
        }
      }

    }
    
    matrix.drawPixel(0, 0, matrix.Color(255,255,255));

    matrix.show();

    while (digitalRead(JOYSTICK_BUTTON)) {
      int x = readJoystickAxis(JOYSTICK_Y);
      int y = readJoystickAxis(JOYSTICK_X);

      if (x == 0 && y == 0) continue;

      int next_cursor_x = cursor_x + x;
      int next_cursor_y = cursor_y + y;

      if (next_cursor_x >= 0 && next_cursor_x < MATRIX_WIDTH && next_cursor_y >= 0 && next_cursor_y < MATRIX_HEIGHT ) {
          
        int previous_x = cursor_x;
        int previous_y = cursor_y;
        
        matrix.drawPixel(cursor_x, cursor_y, matrix.Color(0,0,0));
        cursor_x = next_cursor_x;
        cursor_y = next_cursor_y;
        matrix.drawPixel(cursor_x, cursor_y, matrix.Color(255,255,255));

        switch(g_state.enemy_board[previous_x][previous_y]){
          case EnemyCell::Unknown:
            matrix.drawPixel(previous_x, previous_y, matrix.Color(0, 0, 0));
            break;

          case EnemyCell::Hit:
            matrix.drawPixel(previous_x, previous_y, matrix.Color(255, 255, 0));
            break;

          case EnemyCell::Miss:
            matrix.drawPixel(previous_x, previous_y, matrix.Color(0, 0, 255));
            break;

          case EnemyCell::Sunk:
            matrix.drawPixel(previous_x, previous_y, matrix.Color(255, 0, 0));
            break;
        }
      }
      matrix.show();
      delay(100);
    }

    while (!digitalRead(JOYSTICK_BUTTON)) {
      delay(100);
    }

    switch(battle_shoot(cursor_x, cursor_y)){

      case HitResult::Hit:
        matrix.drawPixel(cursor_x, cursor_y, matrix.Color(255, 255, 0));
        break;

      case HitResult::Water:
        matrix.drawPixel(cursor_x, cursor_y, matrix.Color(0, 0, 255));
        break;

      case HitResult::Sunk:
        matrix.drawPixel(cursor_x, cursor_y, matrix.Color(255, 0, 0));
        break;
    }

    matrix.show();    
    delay(1000);      

  }else{

    matrix.clear();
    for (int i = 0; i < MATRIX_HEIGHT; i++){
      for(int j = 0; j < MATRIX_WIDTH; j++){
        switch (g_state.own_board[j][i]){
          case OwnCell::Empty:
            matrix.drawPixel(j, i, matrix.Color(0, 0, 0));
            break;

          case OwnCell::Hit:
            matrix.drawPixel(j, i, matrix.Color(255, 255, 0));
            break;

          case OwnCell::Miss:
            matrix.drawPixel(j, i, matrix.Color(0, 0, 255));
            break;

          case OwnCell::Sunk:
            matrix.drawPixel(j, i, matrix.Color(255, 0, 0));
            break;

          case OwnCell::Ship:
            matrix.drawPixel(j, i, matrix.Color(255, 255, 255));
            break;
        }
      }

    }

    matrix.show();
    uint8_t opponent_x;
    uint8_t opponent_y;
    HitResult hit = battle_wait_for_opponent_shot(opponent_x, opponent_y);

    

    switch(hit){
      case HitResult::Hit:
        for (int i = 0; i < 3; i++){
          matrix.drawPixel(opponent_x, opponent_y, matrix.Color(0, 0, 0));
          matrix.show();
          delay(100);
          matrix.drawPixel(opponent_x, opponent_y, matrix.Color(255, 255, 0));
          matrix.show();
          delay(100);
        }
        
        break;

      case HitResult::Water:
        for (int i = 0; i < 3; i++){
          matrix.drawPixel(opponent_x, opponent_y, matrix.Color(0, 0, 0));
          matrix.show();
          delay(100);
          matrix.drawPixel(opponent_x, opponent_y, matrix.Color(0, 0, 255));
          matrix.show();
          delay(100);
        }
        break;

      case HitResult::Sunk:
        for (int i = 0; i < 3; i++){
          matrix.drawPixel(opponent_x, opponent_y, matrix.Color(0, 0, 0));
          matrix.show();
          delay(100);
          matrix.drawPixel(opponent_x, opponent_y, matrix.Color(255, 0, 0));
          matrix.show();
          delay(100);
        }
        break;
    }
    delay(1000);
    battle_await_change();

  }

  if(battle_over()){
    current_state = RESULT;
  }
  
}



void result_state(){
  if(my_role == battle_winner()){
    winning_animation();
  }else{
    losing_animation();
  }
  current_state = WAITING;
}

void interface_setup() {

  Serial.begin(115200);
  pinMode(JOYSTICK_BUTTON, INPUT_PULLUP);
  pinMode(JOYSTICK_X, INPUT);
  pinMode(JOYSTICK_Y, INPUT);

  matrix.begin();
  matrix.setBrightness(10);
  battle_begin();
  current_state = WAITING;
}

void interface_loop() {
  switch (current_state) {
    case WAITING:
      waiting();
      break;

    case SETUP:
      setup_state();
      break;

    case PLAYING:
      playing_state();
      break;

    case RESULT:
      result_state();
  }

  delay(250);
}

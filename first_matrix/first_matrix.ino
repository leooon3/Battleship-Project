#include <Adafruit_GFX.h>
#include <Adafruit_NeoMatrix.h>
#include <Adafruit_NeoPixel.h>
#include <map>
#include <vector>

#define JOYSTICK_X 33
#define JOYSTICK_Y 34
#define JOYSTICK_BUTTON 14


#define MATRIX_WIDTH 8
#define MATRIX_HEIGHT 8
#define MATRIX_PIN 12

int cursor_x = 0;
int cursor_y = 0;



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

enum Direction {
  NORTH,
  EAST,
  SOUTH,
  WEST,
};

struct Boat {
  int len;
  Direction dir;
  int initial_pos[2];
};

State current_state;
std::vector<Boat> setup_boat;

void setup() {


  Serial.begin(9600);
  pinMode(JOYSTICK_BUTTON, INPUT_PULLUP);
  pinMode(JOYSTICK_X, INPUT);
  pinMode(JOYSTICK_Y, INPUT);
  // pinMode(LED_BUILTIN, OUTPUT);
  //pinMode(MATRIX_PIN, OUTPUT);



  matrix.begin();
  matrix.setBrightness(10);

  current_state = WAITING;
}

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

  //registrati, aspetta server
  Serial.println("Button pressed!");
  current_state = SETUP;
}


int longest_boat(std::map<int, int> availableBoats) {
  for (auto it = availableBoats.rbegin(); it != availableBoats.rend(); ++it) {
    if (it->second > 0) {
      return it->first;
    }
  }
  return -1;
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

  if ((cursor_y + next_y) < 0 && (cursor_y + next_y) >= MATRIX_HEIGHT) {
    ret = true;
  }
  Serial.println("return value : " + String(ret));

  return ret;
}

void setup_state() {
  std::map<int, int> availableBoats;
  availableBoats[2] = 2;
  availableBoats[3] = 1;
  availableBoats[4] = 1;

  matrix.clear();
  int longest;

  while ((longest = longest_boat(availableBoats)) > 0) {

    Serial.println("longest : " + String(longest));
    while (digitalRead(JOYSTICK_BUTTON)) {
      int x = readJoystickAxis(JOYSTICK_Y);
      int y = readJoystickAxis(JOYSTICK_X);

      int next_cursor_x = cursor_x + x;
      int next_cursor_y = cursor_y + y;

      matrix.drawPixel(cursor_x, cursor_y, matrix.Color(0, 0, 0));

      if (next_cursor_x >= 0 && next_cursor_x < MATRIX_WIDTH) {
        cursor_x += x;
      }

      if (next_cursor_y >= 0 && next_cursor_y < MATRIX_HEIGHT) {
        cursor_y += y;
      }

      matrix.drawPixel(cursor_x, cursor_y, matrix.Color(0, 0, 255));
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

      if (x == 0 && y == 0) {
        continue;
      }

      int next_offset_x = offset_x + x;
      int next_offset_y = offset_y + y;

      if (illegal(next_offset_x, next_offset_y, longest)) {
        delay(100);
        continue;
      }

      if (next_offset_x * x + next_offset_y * y > 0) {
        matrix.drawPixel(cursor_x + next_offset_x, cursor_y + next_offset_y, matrix.Color(0, 0, 255));
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
      length = offset_x;
    } else if (offset_x < 0) {
      dir = WEST;
      length = abs(offset_x);
    } else if (offset_y > 0) {
      dir = NORTH;
      length = offset_y;
    } else {
      dir = SOUTH;
      length = abs(offset_y);
    }

    setup_boat.push_back({ length, dir, { cursor_x, cursor_y } });
  }



  // while(digitalRead(JOYSTICK_BUTTON)){
  //   cursor_movement();
  //   Serial.println("Dentro");
  // }
  //   Serial.println("Esceeeeee");



  // while(digitalRead(JOYSTICK_BUTTON)){
  //   boat_positioning(position);
  // }
}


// the loop function runs over and over again forever
void loop() {
  switch (current_state) {
    case WAITING:
      waiting();
      break;

    case SETUP:
      setup_state();
      break;
  }







  // int button = !digitalRead(JOYSTICK_BUTTON);

  // Serial.print("x: " + String(x));
  // Serial.print("\ty: " + String(y));
  //Serial.println("\tbutton: " + String(button));
  delay(250);
}

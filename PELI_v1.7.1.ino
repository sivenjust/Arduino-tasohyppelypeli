/*
   Arduino tasohyppelyprojekti
   Ryhmä 19
   Johannes Rantapää
   Justus Sivenius
   Perttu Vehviläinen
*/

#include <LiquidCrystal.h>
#include <EEPROM.h>
#include <avr/io.h>
#include <avr/wdt.h>

#define PIN_BUTTON 2
#define PIN_AUTOPLAY 21
#define PIN_READWRITE 11

#define SPRITE_RUN1 1
#define SPRITE_RUN2 2
#define SPRITE_JUMP 3
#define SPRITE_JUMP_UPPER ' '         // Use the '.' character for the head
#define SPRITE_JUMP_LOWER 4
#define SPRITE_TERRAIN_EMPTY ' '      // User the ' ' character
#define SPRITE_TERRAIN_SOLID 5
#define SPRITE_TERRAIN_SOLID_RIGHT 6
#define SPRITE_TERRAIN_SOLID_LEFT 7

#define HERO_HORIZONTAL_POSITION 1    // Horizontal position of hero on screen

#define TERRAIN_WIDTH 16
#define TERRAIN_EMPTY 0
#define TERRAIN_LOWER_BLOCK 1
#define TERRAIN_UPPER_BLOCK 2

#define HERO_POSITION_OFF 0          // Hero is invisible
#define HERO_POSITION_RUN_LOWER_1 1  // Hero is running on lower row (pose 1)
#define HERO_POSITION_RUN_LOWER_2 2  //                              (pose 2)

#define HERO_POSITION_JUMP_1 3       // Starting a jump
#define HERO_POSITION_JUMP_2 4       // Half-way up
#define HERO_POSITION_JUMP_3 5       // Jump is on upper row
#define HERO_POSITION_JUMP_4 6       // Jump is on upper row
#define HERO_POSITION_JUMP_5 7       // Jump is on upper row
#define HERO_POSITION_JUMP_6 8       // Jump is on upper row
#define HERO_POSITION_JUMP_7 9       // Half-way down
#define HERO_POSITION_JUMP_8 10      // About to land

#define HERO_POSITION_RUN_UPPER_1 11 // Hero is running on upper row (pose 1)
#define HERO_POSITION_RUN_UPPER_2 12 //                              (pose 2)

#define EEPROM_HI_SCORE 16  // HiScore easy
#define EEPROM_HI_SCORE2 32 // HiScore hard

LiquidCrystal lcd(12, 11, 10, 6, 5, 4, 3);
static char terrainUpper[TERRAIN_WIDTH + 1];
static char terrainLower[TERRAIN_WIDTH + 1];
static bool buttonPushed = false;

int numberOfLives = 0;
int difficulty = 0;
int speedUp = 0;
int customScoreEasy;
int customScoreHard;
static uint16_t hiScoreHard = 0;
static uint16_t hiScoreEasy = 0;

static bool playing = false;
bool select = false;
bool autoplay = false;
bool soundfx = true;

void initializeGraphics() {
  static byte graphics[] = {
    // Run position 1 // byte 0
    B01111,
    B01101,
    B00111,
    B01101,
    B01101,
    B01100,
    B11010,
    B10011,
    // Run position 2 // byte 1
    B01111,
    B01101,
    B00111,
    B01101,
    B01101,
    B01100,
    B01100,
    B01110,
    // Jump // byte 2
    B01111,
    B01101,
    B00111,
    B11110,
    B01101,
    B01111,
    B10000,
    B00000,
    // Jump lower // byte 3
    B01111,
    B01101,
    B00111,
    B11101,
    B11001,
    B11100,
    B01011,
    B10000,
    // Ground // byte 4
    B10101,
    B11111,
    B11111,
    B11111,
    B11111,
    B11111,
    B11111,
    B11111,
    // Ground right // byte 5
    B00010,
    B00011,
    B00011,
    B00011,
    B00011,
    B00011,
    B00011,
    B00011,
    // Ground left  // byte 6
    B01000,
    B11000,
    B11000,
    B11000,
    B11000,
    B11000,
    B11000,
    B11000,
    // Elephantman head // byte 7
    B00000,
    B00000,
    B00000,
    B00000,
    B01111,
    B01101,
    B00111,
    B01111,
  };

  int i;
  // Skip using character 0, this allows lcd.print() to be used to
  // quickly draw multiple characters
  for (i = 0; i < 8; ++i) {
    lcd.createChar(i + 1, &graphics[i * 8]);
  }
  for (i = 0; i < TERRAIN_WIDTH; ++i) {
    terrainUpper[i] = SPRITE_TERRAIN_EMPTY;
    terrainLower[i] = SPRITE_TERRAIN_EMPTY;
  }
}

void advanceTerrain(char* terrain, byte newTerrain) { // Slide the terrain to the left in half-character increments
  for (int i = 0; i < TERRAIN_WIDTH; ++i) {
    char current = terrain[i];
    char next = (i == TERRAIN_WIDTH - 1) ? newTerrain : terrain[i + 1];
    switch (current) {
      case SPRITE_TERRAIN_EMPTY:
        terrain[i] = (next == SPRITE_TERRAIN_SOLID) ? SPRITE_TERRAIN_SOLID_RIGHT : SPRITE_TERRAIN_EMPTY;
        break;
      case SPRITE_TERRAIN_SOLID:
        terrain[i] = (next == SPRITE_TERRAIN_EMPTY) ? SPRITE_TERRAIN_SOLID_LEFT : SPRITE_TERRAIN_SOLID;
        break;
      case SPRITE_TERRAIN_SOLID_RIGHT:
        terrain[i] = SPRITE_TERRAIN_SOLID;
        break;
      case SPRITE_TERRAIN_SOLID_LEFT:
        terrain[i] = SPRITE_TERRAIN_EMPTY;
        break;
    }
  }
}

bool drawHero(byte position, char* terrainUpper, char* terrainLower, unsigned int score) {
  bool collide = false;
  char upperSave = terrainUpper[HERO_HORIZONTAL_POSITION];
  char lowerSave = terrainLower[HERO_HORIZONTAL_POSITION];
  byte upper, lower;
  switch (position) {
    case HERO_POSITION_OFF:
      upper = lower = SPRITE_TERRAIN_EMPTY;
      break;
    case HERO_POSITION_RUN_LOWER_1:
      upper = SPRITE_TERRAIN_EMPTY;
      lower = SPRITE_RUN1;
      break;
    case HERO_POSITION_RUN_LOWER_2:
      upper = SPRITE_TERRAIN_EMPTY;
      lower = SPRITE_RUN2;
      break;
    case HERO_POSITION_JUMP_1:
    case HERO_POSITION_JUMP_8:
      upper = SPRITE_TERRAIN_EMPTY;
      lower = SPRITE_JUMP;
      break;
    case HERO_POSITION_JUMP_2:
    case HERO_POSITION_JUMP_7:
      upper = SPRITE_JUMP_UPPER;
      lower = SPRITE_JUMP_LOWER;
      break;
    case HERO_POSITION_JUMP_3:
    case HERO_POSITION_JUMP_4:
    case HERO_POSITION_JUMP_5:
    case HERO_POSITION_JUMP_6:
      upper = SPRITE_JUMP;
      lower = SPRITE_TERRAIN_EMPTY;
      break;
    case HERO_POSITION_RUN_UPPER_1:
      upper = SPRITE_RUN1;
      lower = SPRITE_TERRAIN_EMPTY;
      break;
    case HERO_POSITION_RUN_UPPER_2:
      upper = SPRITE_RUN2;
      lower = SPRITE_TERRAIN_EMPTY;
      break;
  }
  if (upper != ' ') {
    terrainUpper[HERO_HORIZONTAL_POSITION] = upper;
    collide = (upperSave == SPRITE_TERRAIN_EMPTY) ? false : true;
  }
  if (lower != ' ') {
    terrainLower[HERO_HORIZONTAL_POSITION] = lower;
    collide |= (lowerSave == SPRITE_TERRAIN_EMPTY) ? false : true;
  }

  byte digits = (score > 9999) ? 5 : (score > 999) ? 4 : (score > 99) ? 3 : (score > 9) ? 2 : 1;

  // life system
  byte heart[] = {
    B01010,
    B11111,
    B11111,
    B01110,
    B00100,
    B00000,
    B00000,
    B00000,
  };
  /*byte skull[] = {
    B01110,
    B10001,
    B11011,
    B10001,
    B01010,
    B01110,
    B00000,
    B00000
    };*/
  lcd.createChar(0, heart);
  //lcd.createChar(0, skull);

  // Draw the scene
  terrainUpper[TERRAIN_WIDTH] = '\0';
  terrainLower[TERRAIN_WIDTH] = '\0';
  char temp = terrainUpper[16 - digits];
  terrainUpper[16 - digits] = '\0';
  terrainLower[16] = '\0';
  lcd.setCursor(0, 0);
  lcd.print(terrainUpper);
  terrainUpper[16 - digits] = temp;
  lcd.setCursor(0, 1);
  lcd.print(terrainLower);

  lcd.setCursor(16 - digits, 0);
  lcd.print(score);

  if (numberOfLives == 2) {
    lcd.setCursor(16 - digits - 2, 0);
    lcd.write((byte)0);
    lcd.setCursor(16 - digits - 3, 0);
    lcd.write((byte)0);
    lcd.setCursor(16 - digits, 0);
    lcd.print(score);
  }
  else if (numberOfLives == 1) {
    lcd.setCursor(16 - digits - 2, 0);
    lcd.write((byte)0);
    lcd.setCursor(16 - digits, 0);
    lcd.print(score);
  }
  else {
    lcd.setCursor(16 - digits, 0);
    lcd.print(score);
  }

  terrainUpper[HERO_HORIZONTAL_POSITION] = upperSave;
  terrainLower[HERO_HORIZONTAL_POSITION] = lowerSave;

  if (difficulty == 1) {  // Saves the score
    customScoreEasy = score;
  }
  else if (difficulty == 2) { // Saves the score
    customScoreHard = score;
  }
  return collide;
}

void buttonPush() { // Handles button press as an interrupt.
  static unsigned long lastInterruptTime = 0; // debounce magic.
  unsigned long interruptTime = millis();
  if (interruptTime - lastInterruptTime > 150)
  {
    buttonPushed = true;
  }
  lastInterruptTime = interruptTime;
}

byte buttons[] = {7, 8, 9,};  // Menu button pins
const byte nrButtons = 3; // Change 3 to 4 if you add a button to menu.
int menusize = 11;
String menu[] = { // Menu structure
  "Tasohyppelypeli",                            //0
  "Tasohyppelypeli>Start game",                 //1
  "Tasohyppelypeli>Start game>Difficulty",      //2
  "Tasohyppelypeli>Start game>Difficulty>Easy", //3
  "Tasohyppelypeli>Start game>Difficulty>Hard", //4
  "Tasohyppelypeli>Settings",                   //5
  "Tasohyppelypeli>Settings>Sound*",            //6
  "Tasohyppelypeli>Settings>Autoplay",          //7
  "Tasohyppelypeli>Settings>HiScore reset",     //8
  "Tasohyppelypeli>Highscore",                  //9
  "Tasohyppelypeli>Credits",                    //10
};

int t;
byte pressedButton, currentPos, currentPosParent, possiblePos[20], possiblePosCount, possiblePosScroll = 0;
String parent = "";

void updateMenu () {
  possiblePosCount = 0;
  while (possiblePosCount == 0) {
    for (t = 1; t < menusize; t++) {
      if (mid(menu[t], 1, inStrRev(menu[t], ">") - 1).equals(menu[currentPos])) {
        possiblePos[possiblePosCount]  =  t;
        possiblePosCount = possiblePosCount + 1;
      }
    }

    //Find the current parent for the current menu
    parent = mid(menu[currentPos], 1, inStrRev(menu[currentPos], ">") - 1);
    currentPosParent = 0;
    for (t = 0; t < menusize; t++) {
      if (parent == menu[t]) {
        currentPosParent = t;
      }
    }

    // Reached the end of the Menu line
    if (possiblePosCount == 0) {
      //Menu Option Items
      switch (currentPos) { // Menu switch-case structure
        case 3: // Difficulty "Easy"
          if (mid(menu[currentPos], len(menu[currentPos]), 1) == "Easy") {
          }
          else {
            difficulty = 1;
            numberOfLives = 2;
            select = true;
          }
          break;
        case 4: // Difficulty "Hard"
          if (mid(menu[currentPos], len(menu[currentPos]), 1) == "Hard") {
          }
          else {
            difficulty = 2;
            numberOfLives = 1;
            select = true;
          }
          break;
        case 6: // Settings "Sound"
          if (mid(menu[currentPos], len(menu[currentPos]), 1) == "*") {
            menu[currentPos] = mid(menu[currentPos], 1, len(menu[currentPos]) - 1);
            soundfx = false;
          }
          else {
            menu[currentPos] = menu[currentPos] + "*";
            soundfx = true;
          }
          break;
        case 7: // Settings "Autoplay"
          if (mid(menu[currentPos], len(menu[currentPos]), 1) == "*") {
            menu[currentPos] = mid(menu[currentPos], 1, len(menu[currentPos]) - 1);
            autoplay = false;
          }
          else {
            menu[currentPos] = menu[currentPos] + "*";
            autoplay = true;
          }
          break;
        case 8: // Settings "HiScore reset"
          if (mid(menu[currentPos], len(menu[currentPos]), 1) == "*") {
            menu[currentPos] = mid(menu[currentPos], 1, len(menu[currentPos]) - 1);
          }
          else {
            menu[currentPos] = menu[currentPos] + "*";
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Resetting...");
            for (int i = 0 ; i < EEPROM.length() ; i++) {
              lcd.setCursor(0, 1);
              EEPROM.write(i, 0);
              lcd.print(i);
            }
            lcd.setCursor(0, 1);
            lcd.print("Done!");
            delay(50);
            reset();
          }
          break;
        case 9: // "Highscore"
          if (mid(menu[currentPos], len(menu[currentPos]), 1) == "gines") {
          }
          else {
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Current HiScore");
            lcd.setCursor(0, 1);
            lcd.print("E:");
            lcd.print(hiScoreEasy);
            lcd.setCursor(8, 1);
            lcd.print("H:");
            lcd.print(hiScoreHard);
            delay(1000);
          }
          break;
        case 10: // "Credits"
          if (mid(menu[currentPos], len(menu[currentPos]), 1) == "gines") {
          }
          else {
            lcd.clear();
            for (int i = 0; i < 2; i++) {
              lcd.setCursor(0, 1);
              lcd.print("Johannes R.");
              delay(500);
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("Johannes R.");
              lcd.setCursor(0, 1);
              lcd.print("Justus S.");
              delay(500);
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("Justus S.");
              lcd.setCursor(0, 1);
              lcd.print("Perttu V.");
              delay(500);
              lcd.clear();
              lcd.setCursor(0, 0);
              lcd.print("Perttu V.");
              delay(500);
              lcd.clear();
            }
          }
          break;
      }
      currentPos = currentPosParent;  // Go to the parent
    }
  }
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(mid(menu[currentPos], inStrRev(menu[currentPos], ">") + 1, len(menu[currentPos]) - inStrRev(menu[currentPos], ">")));
  lcd.setCursor(0, 1); lcd.print(mid(menu[possiblePos[possiblePosScroll]], inStrRev(menu[possiblePos[possiblePosScroll]], ">") + 1, len(menu[possiblePos[possiblePosScroll]]) - inStrRev(menu[possiblePos[possiblePosScroll]], ">")));
}

byte checkButtonPress() { // Detects button presses.
  byte bP = 0;
  byte rBp = 0;
  for (t = 0; t < nrButtons; t++) {
    if (digitalRead(buttons[t]) == 0) {
      bP = (t + 1);
      gameSound(392, 50); // Button press in menu makes a sound
    }
  }
  rBp = bP;
  while (bP != 0) { // wait while the button is still down
    bP = 0;
    for (t = 0; t < nrButtons; t++) {
      if (digitalRead(buttons[t]) == 0) {
        bP = (t + 1);
      }
    }
  }
  return rBp;
}

void setup() {
  Serial.begin(9600);
  pinMode(PIN_READWRITE, OUTPUT);
  digitalWrite(PIN_READWRITE, LOW);
  pinMode(PIN_BUTTON, INPUT_PULLUP);
  digitalWrite(PIN_BUTTON, HIGH);

  attachInterrupt(0/*PIN_BUTTON*/, buttonPush, FALLING);  // Digital pin 2 maps to interrupt 0

  initializeGraphics();

  lcd.begin(16, 2);

  // HiScore stuff
#ifdef RESET_HI_SCORE
  EEPROM.put(EEPROM_HI_SCORE, hiScoreHard);
#endif
  EEPROM.get(EEPROM_HI_SCORE, hiScoreHard);
  if (hiScoreHard == 0xFFFF) hiScoreHard = 0;

#ifdef RESET_HI_SCORE
  EEPROM.put(EEPROM_HI_SCORE2, hiScoreEasy);
#endif
  EEPROM.get(EEPROM_HI_SCORE2, hiScoreEasy);
  if (hiScoreEasy == 0xFFFF) hiScoreEasy = 0;

  for (t = 0; t < nrButtons; t++) {
    pinMode(buttons[t], INPUT_PULLUP);
  }
  lcd.setCursor(0, 0); lcd.print("Ryhma 19");
  lcd.setCursor(0, 1); lcd.print("Tasohyppelypeli");
  delay(1000);
  lcd.clear();
  delay(500);
  EEPROM.put(0, hiScoreHard);
  EEPROM.put(0, hiScoreEasy);
  //startingAnimation();  // Starting animation
  updateMenu();
}

void loop() {
  //debugMode(numberOfLives);
  static byte heroPos = HERO_POSITION_RUN_LOWER_1;
  static byte newTerrainType = TERRAIN_EMPTY;
  static byte newTerrainDuration = 1;
  static bool blink = false;
  static unsigned int distance = 0;
  speedUp++;

  while (!select) { // !select = menu, select = playing
    buttonPress();  // Menu
    updateMenu();
  }

  // If hero dies or is not playing, he starts blinking and instructions print to screen
  if (!playing) {
    drawHero((blink) ? HERO_POSITION_OFF : heroPos, terrainUpper, terrainLower, distance >> 3);
    if (blink) {
      if (autoplay) {
        delay(100);
        buttonPushed = false;
      }
      speedUp = 0;
      if (customScoreHard > hiScoreHard) {
        hiScoreHard = customScoreHard;
        EEPROM.put(EEPROM_HI_SCORE, hiScoreHard);
      }
      else if (customScoreEasy > hiScoreEasy) {
        hiScoreEasy = customScoreEasy;
        EEPROM.put(EEPROM_HI_SCORE2, hiScoreEasy);
      }
      lcd.setCursor(0, 0);
      lcd.print("White = Play");
      lcd.setCursor(0, 1);
      lcd.print("Hold Red = Menu");
      if (digitalRead(9) == LOW) {  // Go to menu
        select = false;
      }
    }
    delay(250);
    blink = !blink;
    if (buttonPushed) {
      gameSound(494, 110);  // Game starting sound
      gameSound(293, 110);
      gameSound(494, 110);
      gameSound(392, 125);
      initializeGraphics();
      heroPos = HERO_POSITION_RUN_LOWER_1;
      playing = true;
      buttonPushed = false;
      distance = 0;
      if (difficulty == 2) {
        numberOfLives = 1;
      }
      else {
        numberOfLives = 2;
      }
    }
    return;
  }

  // Shift the terrain to the left
  advanceTerrain(terrainLower, newTerrainType == TERRAIN_LOWER_BLOCK ? SPRITE_TERRAIN_SOLID : SPRITE_TERRAIN_EMPTY);
  advanceTerrain(terrainUpper, newTerrainType == TERRAIN_UPPER_BLOCK ? SPRITE_TERRAIN_SOLID : SPRITE_TERRAIN_EMPTY);

  if (--newTerrainDuration == 0) {  // Make new terrain to enter on the right
    if (difficulty == 1) {
      if (newTerrainType == TERRAIN_EMPTY) {
        newTerrainType = (random(3) == 0) ? TERRAIN_UPPER_BLOCK : TERRAIN_LOWER_BLOCK;  // Terrain generation for easy difficulty
        newTerrainDuration = 2 + random(20);  // Length of barriers
      }
      else {
        newTerrainType = TERRAIN_EMPTY;
        newTerrainDuration = 12 + random(15); // Length of empty space between barriers
      }
    }
    else {
      if (newTerrainType == TERRAIN_EMPTY) {
        newTerrainType = (random(3) == 0) ? TERRAIN_UPPER_BLOCK : TERRAIN_LOWER_BLOCK;  // Terrain generation for hard difficulty
        newTerrainDuration = 2 + random(12);  // Length of barriers
      }
      else {
        newTerrainType = TERRAIN_EMPTY;
        newTerrainDuration = 10 + random(5); // Length of empty space between barriers
      }
    }
  }

  if (buttonPushed) { // Jump function
    if (heroPos <= HERO_POSITION_RUN_LOWER_2) heroPos = HERO_POSITION_JUMP_1; // Hero jumps
    buttonPushed = false;
    gameSound(494, 50);     // Jump sound
  }

  if (drawHero(heroPos, terrainUpper, terrainLower, distance >> 3)) { // If hero hits an obstacle, he loses a heart
    numberOfLives--;
    while (drawHero(heroPos, terrainUpper, terrainLower, distance >> 3) && numberOfLives >= 0) {  // While hero is inside a barrier
      buttonPushed = false;
      if (heroPos == 1 || heroPos == 2 || heroPos == 9 || heroPos == 10) {  // Blink effect while hero is inside a barrier
        lcd.setCursor(1, 1);
        lcd.print(" ");
      }
      else if (heroPos >= 3 && heroPos <= 9) {  // Blink effect while hero is inside a barrier
        lcd.setCursor(1, 0);
        lcd.print(" ");
      }
      advanceTerrain(terrainLower, newTerrainType == TERRAIN_LOWER_BLOCK ? SPRITE_TERRAIN_SOLID : SPRITE_TERRAIN_EMPTY);
      advanceTerrain(terrainUpper, newTerrainType == TERRAIN_UPPER_BLOCK ? SPRITE_TERRAIN_SOLID : SPRITE_TERRAIN_EMPTY);
      if (difficulty == 1) {
        gameDifficultyEasy();
      }
      if (difficulty == 2) {
        gameDifficultyHard();
      }
      gameSound(98, 30); // Sound heard while hero gets hit
    }
  }
  if (numberOfLives < 0) {  // If hero depletes his hearts, he dies
    playing = false;
    //gameSound(130, 115); // Collision sound, dramatic
    //gameSound(123, 115);
    //gameSound(110, 115);
    //gameSound(92, 250);
    gameSound(523, 100); // Collision sound, melodic
    gameSound(440, 100);
    gameSound(392, 100);
    gameSound(293, 150);
    if (digitalRead(9) == LOW) {  // Return to menu if you hold "back" -button.
      select = false;
    }
  }
  else {
    if (heroPos == HERO_POSITION_RUN_LOWER_2 || heroPos == HERO_POSITION_JUMP_8) {
      heroPos = HERO_POSITION_RUN_LOWER_1;
    }
    else if ((heroPos >= HERO_POSITION_JUMP_3 && heroPos <= HERO_POSITION_JUMP_5) && terrainLower[HERO_HORIZONTAL_POSITION] != SPRITE_TERRAIN_EMPTY) {
      heroPos = HERO_POSITION_RUN_UPPER_1;
    }
    else if (heroPos >= HERO_POSITION_RUN_UPPER_1 && terrainLower[HERO_HORIZONTAL_POSITION] == SPRITE_TERRAIN_EMPTY) {
      heroPos = HERO_POSITION_JUMP_5;
    }
    else if (heroPos == HERO_POSITION_RUN_UPPER_2) {
      heroPos = HERO_POSITION_RUN_UPPER_1;
    }
    else {
      ++heroPos;
    }
    ++distance;
  }
  if (autoplay) {
    gameAutoPlay();
  }
  quitGame();
  gameDifficulty();
  gamePause();
}

int debugMode(int i) {  // Debug mode
  Serial.begin(9600);
  Serial.print(i);
  Serial.println(" Lives");
}

int reset(void) { // Arduino reset function for resetting hiscore
  wdt_enable(WDTO_30MS);
  while (1) {};
}

void gamePause() {  // In-game pause, digital pin 7 pauses, 8 unpauses
  if (digitalRead(7) == LOW) {
    delay(50);
    pinMode(7, OUTPUT);
    digitalWrite(7, LOW);
    while (digitalRead(7 == LOW)) {
      if (digitalRead(8) == LOW) {
        pinMode(7, INPUT_PULLUP);
        break;
      }
    }
  }
}

void gameSound (int pitch, int duration) {   // Sound function
  if (soundfx == true) {
    tone(13, pitch);  // Arduino tone-function, uses digital pin 13
    delay(duration);
    noTone(13);
  }
  else if (!soundfx) {
    noTone(13);
  }
}

void gameDifficulty() { // Handles game difficulty
  if (difficulty == 1) {
    gameDifficultyEasy();
  }
  else if (difficulty == 2) {
    gameDifficultyHard();
  }
}

void quitGame() {
  if (digitalRead(9) == LOW) {
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    playing = false;
  }
}

void gameAutoPlay() { // Autoplay
  if (autoplay) {
    pinMode(PIN_BUTTON, OUTPUT);
    digitalWrite(PIN_BUTTON, terrainLower[HERO_HORIZONTAL_POSITION + 2] == SPRITE_TERRAIN_EMPTY ? HIGH : LOW);
    pinMode(PIN_BUTTON, INPUT_PULLUP);
  }
}

void gameDifficultyEasy() { // Difficulty for easy
  if (speedUp >= 0 && speedUp <= 60) {
    delay(60);
  }
  else if (speedUp >= 60 && speedUp <= 120) {
    delay(55);
  }
  else if (speedUp >= 120 && speedUp <= 180) {
    delay(50);
  }
  else if (speedUp >= 180 && speedUp <= 360) {
    delay(45);
  }
  else if (speedUp >= 360 && speedUp <= 480) {
    delay(40);
  }
  else if (speedUp >= 480 && speedUp <= 600) {
    delay(35);
  }
  else if (speedUp >= 600 && speedUp <= 680) {
    delay(30);
  }
  else if (speedUp >= 680 && speedUp <= 860) {
    delay(25);
  }
  else if (speedUp >= 860 && speedUp <= 1100) {
    delay(20);
  }
  else {
    delay(15);
  }
}

void gameDifficultyHard() { // Difficulty for hard
  if (speedUp >= 0 && speedUp <= 80) {
    delay(50);
  }
  else if (speedUp >= 80 && speedUp <= 160) {
    delay(45);
  }
  else if (speedUp >= 160 && speedUp <= 240) {
    delay(40);
  }
  else if (speedUp >= 240 && speedUp <= 320) {
    delay(35);
  }
  else if (speedUp >= 320 && speedUp <= 480) {
    delay(30);
  }
  else if (speedUp >= 480 && speedUp <= 640) {
    delay(25);
  }
  else if (speedUp >= 640 && speedUp <= 800) {
    delay(20);
  }
  else if (speedUp >= 800 && speedUp <= 1040) {
    delay(15);
  }
  else if (speedUp >= 1040 && speedUp <= 1360) {
    delay(10);
  }
  else if (speedUp >= 1360 && speedUp <= 1760) {
    delay(10);
  }
  else {
  }
}

int buttonPress() { // Menu buttons
  pressedButton = checkButtonPress();
  if (pressedButton != 0) {
    switch (pressedButton) {
      case 1:
        possiblePosScroll = (possiblePosScroll + 1) % possiblePosCount; // Scroll
        break;
      // 4. Button  // 4. Button for scroll back for example.
      //case 4:
      //  possiblePosScroll = (possiblePosScroll + possiblePosCount - 1) % possiblePosCount; // Scroll
      //break;
      case 2:
        currentPos = possiblePos[possiblePosScroll]; //Okay
        break;
      case 3:
        currentPos = currentPosParent; //Back
        possiblePosScroll = 0;
        break;
    }
    updateMenu();
  }
  return pressedButton;
}

String mid(String str, int start, int len) {  // Tallentaa ja palauttaa stringin?
  int t = 0;
  String u = "";
  for (t = 0; t < len; t++) {
    u = u + str.charAt(t + start - 1);
  }
  return u;
}

int inStrRev(String str, String chr) {  // Finds and returns a char from string
  int t = str.length() - 1;
  int u = 0;
  while (t > -1) {
    if (str.charAt(t) == chr.charAt(0)) {
      u = t + 1; t = -1;
    }
    t = t - 1;
  }
  return u;
}

int len(String str) { // Returns string length
  return str.length();
}

void startingAnimation() {  // Starting animation
  int x;
  int y;

  for (x = 0; x <= 7; x++) {
    lcd.setCursor(x, 1);
    if (x % 2 == 1 && x != 6) {
      gameSound(294, 100);
      lcd.write((byte)1);
    }
    else if (x % 2 == 0 && x != 6) {
      gameSound(294, 100);
      lcd.write((byte)2);
    }
    else {
      lcd.setCursor(x, 0);
      gameSound(440, 100);
      lcd.write((byte)3);
    }
    delay(300);
    lcd.clear();
  }

  lcd.setCursor(8, 1);
  gameSound(294, 100);
  lcd.write((byte)2);
  delay(600);
  lcd.clear();
  lcd.setCursor(0, 1);
  gameSound(130, 100);
  lcd.write((byte)6); // Half wall
  lcd.setCursor(8, 1);
  // gameSound(130, 100);
  lcd.write((byte)2); // Walking animation
  delay(300);
  lcd.clear();
  for (x = 1; x < 7; x++) {
    lcd.setCursor(x, 1);
    gameSound(130, 100);
    lcd.write((byte)5);     // Whole wall
    if (x == 2) {
      x++;
    }
    if (x == 4) {
      x++;
      x++;
    }
    lcd.setCursor(8, 1);
    lcd.write((byte)2);
    delay(300);
    lcd.clear();
  }
  for (x = 10; x < 16; x++) {
    lcd.setCursor(7, 1);
    lcd.write((byte)5);
    if (x == 11) {
      x++;
    }
    if (x < 14) {
      lcd.setCursor(x, 0);
      gameSound(587, 100);
    }
    else {
      lcd.setCursor(x, 1);
      gameSound(392, 100);
    }
    //gameSound(392, 100);
    lcd.write((byte)8);
    delay(300);
    lcd.clear();
  }
  lcd.setCursor(7, 1);
  lcd.write((byte)5);
  delay(400);
  lcd.clear();
  delay(500);
  lcd.clear();
  lcd.print("The miserable");
  delay(100);
  lcd.setCursor(0, 1);
  lcd.print("adventures of");
  delay(1000);
  lcd.clear();
  lcd.setCursor(2, 0);
  lcd.print("THE ELEPHANT");
  lcd.setCursor(6, 1);
  lcd.print("MAN");
  gameSound(293, 200);       // Animation intro soundtrack
  delay(20);
  gameSound(293, 100);
  delay(20);
  gameSound(293, 100);
  delay(20);
  gameSound(440, 200);
  gameSound(494, 200);
  delay(20);
  gameSound(392, 200);
  delay(1000);
  delay(2000);
  lcd.clear();
  delay(1000);

  while (buttonPushed == false) {
    // 1
    lcd.setCursor(5, 0);
    lcd.print("Hold");
    lcd.setCursor(4, 1);
    lcd.print("-Jump-");
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(14, 1);
    lcd.write((byte)1);
    gameSound(65, 100);        // Press jump Music
    delay(300);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(14, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(500);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(14, 1);
    lcd.write((byte)1);
    gameSound(65, 100);
    delay(100);
    lcd.setCursor(14, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(300);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(14, 1);
    lcd.write((byte)1);
    gameSound(65, 100);
    delay(300);
    if (digitalRead(2) == LOW) {
      break;
    } lcd.setCursor(14, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(200);
    lcd.setCursor(14, 1);
    lcd.write((byte)1);
    gameSound(220, 100);
    delay(200);
    if (digitalRead(2) == LOW) {
      break;
    } lcd.setCursor(14, 1);
    lcd.write((byte)2);
    gameSound(65, 100);
    delay(100);
    lcd.setCursor(14, 1);
    lcd.write((byte)1);
    gameSound(220, 100);
    delay(300);
    if (digitalRead(2) == LOW) {
      break; // 2
    }
    lcd.setCursor(14, 1);
    lcd.write((byte)2);
    gameSound(65, 100);
    delay(300);
    lcd.setCursor(14, 1);
    lcd.write((byte)1);
    gameSound(220, 100);
    delay(500);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(14, 1);
    lcd.write((byte)2);
    gameSound(65, 100);
    delay(100);
    lcd.setCursor(14, 1);
    lcd.write((byte)1);
    gameSound(220, 100);
    delay(300);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(14, 1);
    lcd.write((byte)2);
    gameSound(65, 100);
    delay(300);
    if (digitalRead(2) == LOW) {
      break;
    }  lcd.setCursor(14, 1);
    lcd.write((byte)1);
    gameSound(220, 100);
    delay(200);

    lcd.setCursor(14, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(200);
    if (digitalRead(2) == LOW) {
      break;
    }   lcd.setCursor(14, 1);
    lcd.write((byte)1);
    gameSound(65, 100);
    delay(100);
    lcd.setCursor(14, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(300);
    if (digitalRead(2) == LOW) {
      break; // 3
    }
    lcd.setCursor(14, 1);
    lcd.write((byte)1);
    gameSound(65, 100);
    delay(300);
    lcd.setCursor(14, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(500);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(14, 1);
    lcd.write((byte)1);
    gameSound(65, 100);
    delay(100);
    lcd.setCursor(14, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(300);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(14, 1);
    lcd.write((byte)1);
    gameSound(65, 100);
    delay(300);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(14, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(200);
    lcd.clear();

    lcd.setCursor(6, 0);
    lcd.print("Hold");
    lcd.setCursor(5, 1);
    lcd.print("-Jump-");
    lcd.setCursor(14, 0);
    lcd.write((byte)3);
    gameSound(220, 100);
    delay(200);

    if (digitalRead(2) == LOW) {
      break;
    }

    lcd.clear();

    lcd.setCursor(6, 0);
    lcd.print("Hold");
    lcd.setCursor(5, 1);
    lcd.print("-Jump-");
    lcd.setCursor(1, 0);
    lcd.write((byte)3);

    gameSound(65, 100);
    delay(100);

    lcd.clear();

    lcd.setCursor(6, 0);
    lcd.print("Hold");
    lcd.setCursor(5, 1);
    lcd.print("-Jump-");
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    gameSound(220, 100);
    delay(100);
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    gameSound(65, 100);
    delay(100);
    if (digitalRead(2) == LOW) {
      break; // 4
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    gameSound(65, 100);
    delay(300);
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(500);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    gameSound(65, 100);
    delay(100);
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(300);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    gameSound(65, 100);
    delay(100);
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    gameSound(65, 100);
    delay(100);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    gameSound(220, 100);
    delay(100);
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(100);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    gameSound(65, 100);
    delay(100);
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(100);
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    gameSound(65, 50);
    delay(50);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    gameSound(220, 50);
    delay(50);
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    gameSound(220, 50);
    delay(50);
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    gameSound(65, 50);
    delay(50);
    if (digitalRead(2) == LOW) {
      break; // 5
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    gameSound(65, 100);
    delay(300);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(500);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    gameSound(65, 100);
    delay(100);
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(300);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    gameSound(65, 100);
    delay(300);
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(200);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    gameSound(220, 100);
    delay(200);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    gameSound(65, 100);
    delay(100);
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    gameSound(220, 100);
    delay(300);
    if (digitalRead(2) == LOW) {
      break; // 6
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    gameSound(65, 100);
    delay(300);
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    gameSound(220, 100);
    delay(500);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    gameSound(65, 100);
    delay(100);
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    gameSound(220, 100);
    delay(300);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    gameSound(65, 100);
    delay(300);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    gameSound(220, 100);
    delay(200);
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(200);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    gameSound(65, 100);
    delay(100);
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(300);
    if (digitalRead(2) == LOW) {
      break; // 7
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    gameSound(65, 100);
    delay(300);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(500);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    gameSound(65, 100);
    delay(100);
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(300);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    gameSound(65, 100);
    delay(300);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(200);
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    lcd.setCursor(14, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(200);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    lcd.setCursor(14, 1);
    lcd.write((byte)1);
    gameSound(65, 100);
    delay(100);
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    lcd.setCursor(14, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(300);
    if (digitalRead(2) == LOW) {
      break; // 8
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    lcd.setCursor(14, 1);
    lcd.write((byte)1);
    gameSound(65, 100);
    delay(300);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    lcd.setCursor(14, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(500);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    lcd.setCursor(14, 1);
    lcd.write((byte)1);
    gameSound(65, 100);
    delay(100);
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    lcd.setCursor(14, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(300);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    lcd.setCursor(14, 1);
    lcd.write((byte)1);
    gameSound(65, 100);
    delay(100);
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    lcd.setCursor(14, 1);
    lcd.write((byte)2);
    gameSound(65, 100);
    delay(100);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    lcd.setCursor(14, 1);
    lcd.write((byte)1);
    gameSound(220, 100);
    delay(100);
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    lcd.setCursor(14, 1);
    lcd.write((byte)2);
    gameSound(220, 100);
    delay(100);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    lcd.setCursor(14, 1);
    lcd.write((byte)1);
    gameSound(65, 100);
    delay(100);
    lcd.clear();
    lcd.clear();
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    lcd.setCursor(14, 1);
    lcd.write((byte)1);
    lcd.setCursor(3, 0);
    lcd.write((byte)2);
    lcd.setCursor(12, 0);
    lcd.write((byte)1);
    gameSound(220, 100);
    delay(100);
    if (digitalRead(2) == LOW) {
      break;
    }
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    lcd.setCursor(14, 1);
    lcd.write((byte)2);
    lcd.setCursor(3, 0);
    lcd.write((byte)1);
    lcd.setCursor(5, 1);
    lcd.write((byte)1);
    lcd.setCursor(12, 0);
    lcd.write((byte)2);
    lcd.setCursor(10, 1);
    lcd.write((byte)2);
    gameSound(65, 50);
    delay(50);
    lcd.setCursor(1, 1);
    lcd.write((byte)2);
    lcd.setCursor(14, 1);
    lcd.write((byte)1);
    lcd.setCursor(3, 0);
    lcd.write((byte)2);
    lcd.setCursor(5, 1);
    lcd.write((byte)2);
    lcd.setCursor(7, 0);
    lcd.write((byte)2);
    lcd.setCursor(12, 0);
    lcd.write((byte)1);
    lcd.setCursor(10, 1);
    lcd.write((byte)1);
    lcd.setCursor(8, 0);
    lcd.write((byte)1);
    gameSound(220, 50);
    delay(50);
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    lcd.setCursor(14, 1);
    lcd.write((byte)2);
    lcd.setCursor(3, 0);
    lcd.write((byte)1);
    lcd.setCursor(5, 1);
    lcd.write((byte)2);
    lcd.setCursor(7, 0);
    lcd.write((byte)1);
    lcd.setCursor(12, 0);
    lcd.write((byte)2);
    lcd.setCursor(10, 1);
    lcd.write((byte)2);
    lcd.setCursor(8, 0);
    lcd.write((byte)2);
    gameSound(65, 50);
    delay(50);
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    lcd.setCursor(14, 1);
    lcd.write((byte)2);
    lcd.setCursor(3, 0);
    lcd.write((byte)1);
    lcd.setCursor(5, 1);
    lcd.write((byte)1);
    lcd.setCursor(7, 0);
    lcd.write((byte)1);
    lcd.setCursor(12, 0);
    lcd.write((byte)2);
    lcd.setCursor(10, 1);
    lcd.write((byte)2);
    lcd.setCursor(8, 0);
    lcd.write((byte)2);
    gameSound(65, 50);
    delay(50);
    lcd.setCursor(1, 1);
    lcd.write((byte)1);
    lcd.setCursor(14, 1);
    lcd.write((byte)2);
    lcd.setCursor(3, 0);
    lcd.write((byte)1);
    lcd.setCursor(5, 1);
    lcd.write((byte)1);
    lcd.setCursor(7, 0);
    lcd.write((byte)1);
    lcd.setCursor(12, 0);
    lcd.write((byte)2);
    lcd.setCursor(10, 1);
    lcd.write((byte)2);
    lcd.setCursor(8, 0);
    lcd.write((byte)2);
    //gameSound(65, 50);
    //delay(50);
    lcd.clear();
  }
}

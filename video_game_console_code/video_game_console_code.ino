#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Wire.h>

// ===== DISPLAY SETTINGS =====
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// ===== PIN DEFINITIONS =====
#define JOY_X 34
#define JOY_Y 35
#define FIRE_BTN 25
#define SHIELD_BTN 26
#define OK_BTN 27
#define RESET_BTN 14

// ===== GAME STATES =====
enum GameState {
  STATE_MENU,
  STATE_ROBODODGE,
  STATE_TICTACTOE,
  STATE_PAUSE,
  STATE_CONFIRM_RESET,
  STATE_GAME_OVER
};

GameState gameState = STATE_MENU;

// ===== STRUCTS =====
struct Player {
  int x;
  bool shieldActive;
  int shieldCharge;     // 0–5
};

struct Bullet {
  int x, y;
  bool active;
};

struct Enemy {
  int x, y;
  int speed;
  bool active;
};

// ===== CONSTANTS =====
#define MAX_BULLETS 5
#define MAX_ENEMIES 6
#define ENEMY_SPAWN_INTERVAL 700
#define PLAYER_Y (SCREEN_HEIGHT - 10)
#define SHIELD_FULL 5

// ===== GLOBALS =====
Player player;
Bullet bullets[MAX_BULLETS];
Enemy enemies[MAX_ENEMIES];
int score = 0;
unsigned long lastEnemySpawn = 0;
unsigned long lastFrame = 0;
int menuIndex = 0; // 0 = RoboDodge, 1 = TicTacToe
bool resetChoiceYes = true; // confirm screen selector
unsigned long resetPressTime = 0;
bool resetHeld = false;

// ===== INPUT HELPERS =====
bool readButton(int pin) { return digitalRead(pin) == LOW; }

void waitRelease(int pin) {
  while (digitalRead(pin) == LOW) delay(10);
}

// ===== INITIAL SETUP =====
void setup() {
  pinMode(FIRE_BTN, INPUT_PULLUP);
  pinMode(SHIELD_BTN, INPUT_PULLUP);
  pinMode(OK_BTN, INPUT_PULLUP);
  pinMode(RESET_BTN, INPUT_PULLUP);
  analogReadResolution(10);

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 30);
  display.println(F("Display OK"));
  display.display();
  randomSeed(analogRead(0));

  resetGame();

  gameState = STATE_MENU;
  Serial.println("Starting in MENU");

}

// ===== RESET GAME =====
void resetGame() {
  player.x = SCREEN_WIDTH / 2;
  player.shieldActive = false;
  player.shieldCharge = 0;
  score = 0;
  for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = false;
  for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = false;
  lastEnemySpawn = millis();
  gameState = STATE_ROBODODGE;
}

// ===== DRAW MENU =====
void drawMenu() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  // Robot icon (left)
  display.drawRect(20, 20, 20, 20, SSD1306_WHITE);
  display.drawPixel(30, 18, SSD1306_WHITE);
  display.drawLine(28, 24, 32, 24, SSD1306_WHITE);
  display.fillRect(25, 28, 10, 2, SSD1306_WHITE); // eyes
  display.drawRect(23, 32, 14, 2, SSD1306_WHITE); // mouth
  display.setCursor(15, 44);
  display.print("RoboDodge");

  // TicTacToe icon (right)
  int ox = 80;
  display.drawRect(ox, 20, 20, 20, SSD1306_WHITE);
  display.drawLine(ox + 6, 20, ox + 6, 40, SSD1306_WHITE);
  display.drawLine(ox + 13, 20, ox + 13, 40, SSD1306_WHITE);
  display.drawLine(ox, 27, ox + 20, 27, SSD1306_WHITE);
  display.drawLine(ox, 33, ox + 20, 33, SSD1306_WHITE);
  display.setCursor(72, 44);
  display.print("TicTacToe");

  // Selection box
  if (menuIndex == 0) display.drawRect(18, 18, 24, 24, SSD1306_WHITE);
  else display.drawRect(78, 18, 24, 24, SSD1306_WHITE);

  display.display();
}

// ===== UPDATE MENU =====
void updateMenu() {
  int joy = analogRead(JOY_X);
  if (joy < 400) menuIndex = 0;
  if (joy > 600) menuIndex = 1;

  if (readButton(OK_BTN)) {
    if (menuIndex == 0) {
      resetGame(); // RoboDodge
    } else {
      startTicTacToe(); // TicTacToe launch function
    }
    waitRelease(OK_BTN);
  }
}

// ===== DRAW PLAYER =====
void drawPlayer() {
  int x = player.x;
  int y = PLAYER_Y;
  display.drawRect(x - 3, y - 4, 6, 6, SSD1306_WHITE); // body
  display.drawPixel(x, y - 6, SSD1306_WHITE);          // antenna
  display.drawPixel(x - 1, y - 3, SSD1306_WHITE);
  display.drawPixel(x + 1, y - 3, SSD1306_WHITE);
  display.drawLine(x - 2, y - 1, x + 2, y - 1, SSD1306_WHITE); // mouth
}

// ===== SPAWN ENEMY =====
void spawnEnemy() {
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!enemies[i].active) {
      enemies[i].x = random(5, SCREEN_WIDTH - 5);
      enemies[i].y = 0;
      enemies[i].speed = random(1, 3);
      enemies[i].active = true;
      return;
    }
  }
}

// ===== DRAW ENEMIES =====
void drawEnemies() {
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (enemies[i].active)
      display.drawCircle(enemies[i].x, enemies[i].y, 2, SSD1306_WHITE);
  }
}

// ===== DRAW BULLETS =====
void drawBullets() {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].active)
      display.drawFastVLine(bullets[i].x, bullets[i].y, 3, SSD1306_WHITE);
  }
}

// ===== UPDATE GAME =====
void updateGame() {
  int joy = analogRead(JOY_X);
  player.x = map(joy, 0, 1023, 5, SCREEN_WIDTH - 5);

  // Fire
  if (readButton(FIRE_BTN)) {
    for (int i = 0; i < MAX_BULLETS; i++) {
      if (!bullets[i].active) {
        bullets[i].x = player.x;
        bullets[i].y = PLAYER_Y - 6;
        bullets[i].active = true;
        break;
      }
    }
  }

  // Shield
  if (readButton(SHIELD_BTN) && player.shieldCharge >= SHIELD_FULL) {
    player.shieldActive = true;
    player.shieldCharge = 0;
  }

  // OK -> Pause
  if (readButton(OK_BTN)) {
    waitRelease(OK_BTN);
    gameState = STATE_PAUSE;
    return;
  }

  // RESET -> Confirm
  if (readButton(RESET_BTN)) {
    waitRelease(RESET_BTN);
    gameState = STATE_CONFIRM_RESET;
    return;
  }

  // Long press reset for menu
  if (digitalRead(RESET_BTN) == LOW) {
    if (!resetHeld) {
      resetHeld = true;
      resetPressTime = millis();
    } else if (millis() - resetPressTime > 1500) {
      gameState = STATE_MENU;
      resetHeld = false;
      return;
    }
  } else {
    resetHeld = false;
  }

  // Update bullets
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].active) {
      bullets[i].y -= 3;
      if (bullets[i].y < 0) bullets[i].active = false;
    }
  }

  // Spawn enemies
  if (millis() - lastEnemySpawn > ENEMY_SPAWN_INTERVAL) {
    spawnEnemy();
    lastEnemySpawn = millis();
  }

  // Update enemies + collisions
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (enemies[i].active) {
      enemies[i].y += enemies[i].speed;
      if (enemies[i].y > SCREEN_HEIGHT) enemies[i].active = false;

      // Bullet collision
      for (int j = 0; j < MAX_BULLETS; j++) {
        if (bullets[j].active && abs(bullets[j].x - enemies[i].x) < 4 && abs(bullets[j].y - enemies[i].y) < 4) {
          enemies[i].active = false;
          bullets[j].active = false;
          score++;
          player.shieldCharge = min(SHIELD_FULL, player.shieldCharge + 1);
        }
      }

      // Player collision
      if (abs(enemies[i].x - player.x) < 5 && abs(enemies[i].y - PLAYER_Y) < 5) {
        if (player.shieldActive) {
          player.shieldActive = false;
          enemies[i].active = false;
        } else {
          gameState = STATE_GAME_OVER;
        }
      }
    }
  }
}

// ===== DRAW SHIELD BAR =====
void drawShieldBar() {
  int barWidth = map(player.shieldCharge, 0, SHIELD_FULL, 0, 20);
  display.drawRect(SCREEN_WIDTH - 25, 2, 20, 5, SSD1306_WHITE);
  display.fillRect(SCREEN_WIDTH - 25, 2, barWidth, 5, SSD1306_WHITE);
  if (player.shieldActive) {
    display.drawCircle(SCREEN_WIDTH - 35, 5, 2, SSD1306_WHITE);
  }
}

// ===== DRAW GAME =====
void drawGame() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print("Score: ");
  display.print(score);

  drawShieldBar();
  drawEnemies();
  drawBullets();
  drawPlayer();

  display.display();
}

// ===== PAUSE SCREEN =====
void drawPauseScreen() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(35, 20);
  display.print("PAUSED");
  display.display();

  if (readButton(OK_BTN)) {
    waitRelease(OK_BTN);
    gameState = STATE_ROBODODGE;
  }
}

// ===== CONFIRM RESET =====
void drawConfirmReset() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(25, 15);
  display.print("RESET GAME?");
  display.setCursor(35, 35);
  display.print(resetChoiceYes ? "> YES" : "  YES");
  display.setCursor(85, 35);
  display.print(!resetChoiceYes ? "> NO" : "  NO");
  display.display();

  int joy = analogRead(JOY_X);
  if (joy < 400) resetChoiceYes = true;
  if (joy > 600) resetChoiceYes = false;

  if (readButton(OK_BTN)) {
    waitRelease(OK_BTN);
    if (resetChoiceYes) resetGame();
    else gameState = STATE_ROBODODGE;
  }
}

// ===== GAME OVER =====
void drawGameOver() {
  display.clearDisplay();
  display.setTextSize(2);
  display.setCursor(15, 20);
  display.print("GAME OVER");
  display.setTextSize(1);
  display.setCursor(30, 45);
  display.print("Score: ");
  display.print(score);
  display.display();

  if (readButton(OK_BTN) || readButton(RESET_BTN)) {
    waitRelease(OK_BTN);
    waitRelease(RESET_BTN);
    resetGame();
  }

  if (readButton(RESET_BTN)) {
    waitRelease(RESET_BTN);
    gameState = STATE_MENU;
  }
}

// ===== TICTACTOE GAME =====

#define GRID_SIZE 3

char board[GRID_SIZE][GRID_SIZE];
int cursorX = 0;
int cursorY = 0;
bool xTurn = true;
bool gameOver = false;
char winner = ' ';

// ----- Start New Game -----
void startTicTacToe() {
  for (int i = 0; i < GRID_SIZE; i++)
    for (int j = 0; j < GRID_SIZE; j++)
      board[i][j] = ' ';
  cursorX = 0;
  cursorY = 0;
  xTurn = true;
  gameOver = false;
  winner = ' ';
  gameState = STATE_TICTACTOE;
}

// ----- Check Winner -----
char checkWinner() {
  for (int i = 0; i < 3; i++) {
    if (board[i][0] != ' ' && board[i][0] == board[i][1] && board[i][1] == board[i][2])
      return board[i][0];
    if (board[0][i] != ' ' && board[0][i] == board[1][i] && board[1][i] == board[2][i])
      return board[0][i];
  }
  if (board[0][0] != ' ' && board[0][0] == board[1][1] && board[1][1] == board[2][2])
    return board[0][0];
  if (board[0][2] != ' ' && board[0][2] == board[1][1] && board[1][1] == board[2][0])
    return board[0][2];
  bool draw = true;
  for (int i = 0; i < 3; i++)
    for (int j = 0; j < 3; j++)
      if (board[i][j] == ' ') draw = false;
  return draw ? 'D' : ' ';
}

// ----- Update -----
void updateTicTacToe() {
  if (gameOver) {
    if (readButton(RESET_BTN) || readButton(OK_BTN)) {
      waitRelease(RESET_BTN);
      waitRelease(OK_BTN);
      startTicTacToe();
    }
    if (readButton(RESET_BTN)) {
      waitRelease(RESET_BTN);
      gameState = STATE_MENU;
    }
    return;
  }

  // Joystick movement
  int joyX = analogRead(JOY_X);
  int joyY = analogRead(JOY_Y); // use Y for up/down navigation
  if (joyX < 400 && cursorX > 0) cursorX--;
  if (joyX > 600 && cursorX < 2) cursorX++;
  if (joyY < 400 && cursorY > 0) cursorY--;
  if (joyY > 600 && cursorY < 2) cursorY++;

  // X move (Fire button)
  if (readButton(FIRE_BTN)) {
    if (xTurn && board[cursorY][cursorX] == ' ') {
      board[cursorY][cursorX] = 'X';
      xTurn = false;
    }
    waitRelease(FIRE_BTN);
  }

  // O move (Shield button)
  if (readButton(SHIELD_BTN)) {
    if (!xTurn && board[cursorY][cursorX] == ' ') {
      board[cursorY][cursorX] = 'O';
      xTurn = true;
    }
    waitRelease(SHIELD_BTN);
  }

  // Pause
  if (readButton(OK_BTN)) {
    waitRelease(OK_BTN);
    gameState = STATE_PAUSE;
    return;
  }

  // Reset confirmation
  if (readButton(RESET_BTN)) {
    waitRelease(RESET_BTN);
    gameState = STATE_CONFIRM_RESET;
    return;
  }

  // Long-press reset for menu
  if (digitalRead(RESET_BTN) == LOW) {
    if (!resetHeld) {
      resetHeld = true;
      resetPressTime = millis();
    } else if (millis() - resetPressTime > 1500) {
      gameState = STATE_MENU;
      resetHeld = false;
      return;
    }
  } else {
    resetHeld = false;
  }

  // Check winner
  char result = checkWinner();
  if (result != ' ') {
    gameOver = true;
    winner = result;
  }
}

// ----- Draw Cell Mark -----
void drawMark(int x, int y, char mark) {
  int cx = 20 + x * 30;
  int cy = 20 + y * 30;
  if (mark == 'X') {
    display.drawLine(cx - 8, cy - 8, cx + 8, cy + 8, SSD1306_WHITE);
    display.drawLine(cx - 8, cy + 8, cx + 8, cy - 8, SSD1306_WHITE);
  } else if (mark == 'O') {
    display.drawCircle(cx, cy, 8, SSD1306_WHITE);
  }
}

// ----- Draw Game -----
void drawTicTacToe() {
  display.clearDisplay();

  // Turn indicator (X top-left, O top-right)
  if (xTurn) {
    display.fillRect(0, 0, 10, 10, SSD1306_WHITE);
    display.drawCircle(SCREEN_WIDTH - 5, 5, 5, SSD1306_WHITE);
  } else {
    display.drawRect(0, 0, 10, 10, SSD1306_WHITE);
    display.fillCircle(SCREEN_WIDTH - 5, 5, 5, SSD1306_WHITE);
  }

  // Draw grid
  for (int i = 1; i < 3; i++) {
    display.drawLine(20, 20 + i * 30 - 15, 110, 20 + i * 30 - 15, SSD1306_WHITE);
    display.drawLine(20 + i * 30 - 15, 5, 20 + i * 30 - 15, 95, SSD1306_WHITE);
  }

  // Draw marks
  for (int y = 0; y < 3; y++)
    for (int x = 0; x < 3; x++)
      if (board[y][x] != ' ') drawMark(x, y, board[y][x]);

  // Draw cursor
  int cx = 20 + cursorX * 30;
  int cy = 20 + cursorY * 30;
  display.drawRect(cx - 10, cy - 10, 20, 20, SSD1306_WHITE);

  // Game result
  if (gameOver) {
    display.setTextSize(1);
    display.setCursor(25, 55);
    if (winner == 'D') display.print("DRAW!");
    else {
      display.print("WINNER: ");
      display.print(winner);
    }
  }

  display.display();
}


// ===== MAIN LOOP =====
void loop() {
  Serial.println(gameState);
  delay(1000);
  unsigned long now = millis();
  if (now - lastFrame < 33) return; // ~30 FPS
  lastFrame = now;

  switch (gameState) {
    case STATE_MENU: updateMenu(); drawMenu(); break;
    case STATE_ROBODODGE: updateGame(); drawGame(); break;
    case STATE_TICTACTOE: updateTicTacToe(); drawTicTacToe(); break;
    case STATE_PAUSE: drawPauseScreen(); break;
    case STATE_CONFIRM_RESET: drawConfirmReset(); break;
    case STATE_GAME_OVER: drawGameOver(); break;
  }
}

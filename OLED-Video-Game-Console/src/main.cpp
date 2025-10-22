#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//  OLED SETTINGS 
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDR 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

//  BUTTON PINS 
#define BTN_RED     D5   // 🔴 Up
#define BTN_WHITE   D7   // ⚪ Down
#define BTN_YELLOW  D0   // 🟡 Left / Shift key
#define BTN_BLUE    D6   // 🔵 Right / OK

inline bool pressed(int pin) { return digitalRead(pin) == LOW; }
inline void waitRelease(int pin) { while (digitalRead(pin) == LOW) delay(5); }

//  GAME STATE 
enum GameState {
  STATE_MENU,
  STATE_ROBODODGE,
  STATE_TICTACTOE,
  STATE_PAUSE_ROBODODGE,
  STATE_PAUSE_TICTACTOE,
  STATE_GAME_OVER
};
GameState gameState = STATE_MENU;

//  ROBO DODGE 
struct Player { int x; } player;
struct Bullet { int x, y; bool active; };
struct Enemy { int x, y, speed; bool active; };
#define MAX_BULLETS 5
#define MAX_ENEMIES 6
Bullet bullets[MAX_BULLETS];
Enemy enemies[MAX_ENEMIES];
int score = 0;
unsigned long lastEnemySpawn = 0;

//  TICTACTOE 
#define GRID_SIZE 3
char board[GRID_SIZE][GRID_SIZE];
int cursorX = 0, cursorY = 0;
bool xTurn = true;
bool tttGameOver = false;
char tttWinner = ' ';
bool tttDraw = false;

//  MENU 
int menuIndex = 0; // 0 = RoboDodge, 1 = TicTacToe
int pauseMenuIndex = 0; // 0 = Resume, 1 = Exit
unsigned long lastFrame = 0;

// Shift key state tracking
bool yellowShiftActive = false;
bool whiteShiftActive = false;

//  SETUP 
void resetRoboDodge();
void drawMenu();
void updateMenu();
void updateRoboDodge();
void drawRoboDodge();
void startTicTacToe();
void updateTicTacToe();
void drawTicTacToe();
void checkTicTacToeWin();
void drawPauseMenu();
void updatePauseMenu(bool fromRoboDodge);

void setup() {
  Serial.begin(115200);
  Wire.begin();

  pinMode(BTN_RED, INPUT_PULLUP);
  pinMode(BTN_WHITE, INPUT_PULLUP);
  pinMode(BTN_YELLOW, INPUT_PULLUP);
  pinMode(BTN_BLUE, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDR)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }

  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  randomSeed(analogRead(A0));

  resetRoboDodge();
  startTicTacToe();
}

//  MAIN MENU 
void drawMenu() {
  display.clearDisplay();
  
  // Title
  display.setTextSize(2);
  display.setCursor(10, 5);
  display.println(F("CONSOLE"));
  
  // Draw line separator
  display.drawLine(0, 22, SCREEN_WIDTH, 22, SSD1306_WHITE);
  
  // Game options
  display.setTextSize(1);
  
  // RoboDodge option
  if (menuIndex == 0) {
    display.fillRect(4, 28, 120, 12, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  } else {
    display.setTextColor(SSD1306_WHITE);
  }
  display.setCursor(10, 30);
  display.println(F("RoboDodge"));
  
  // TicTacToe option
  if (menuIndex == 1) {
    display.fillRect(4, 44, 120, 12, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  } else {
    display.setTextColor(SSD1306_WHITE);
  }
  display.setCursor(10, 46);
  display.println(F("TicTacToe"));
  
  display.setTextColor(SSD1306_WHITE);
  display.display();
}

void updateMenu() {
  // Red = Up
  if (pressed(BTN_RED)) {
    menuIndex = max(0, menuIndex - 1);
    waitRelease(BTN_RED);
  }
  // White = Down
  if (pressed(BTN_WHITE)) {
    menuIndex = min(1, menuIndex + 1);
    waitRelease(BTN_WHITE);
  }
  // Blue = OK
  if (pressed(BTN_BLUE)) {
    waitRelease(BTN_BLUE);
    if (menuIndex == 0) {
      resetRoboDodge();
      gameState = STATE_ROBODODGE;
    } else {
      startTicTacToe();
      gameState = STATE_TICTACTOE;
    }
  }
}

//  ROBO DODGE 
void resetRoboDodge() {
  player.x = SCREEN_WIDTH / 2;
  score = 0;
  for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = false;
  for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = false;
  lastEnemySpawn = millis();
}

void spawnEnemy() {
  for (int i = 0; i < MAX_ENEMIES; i++) if (!enemies[i].active) {
    enemies[i].x = random(8, SCREEN_WIDTH - 8);
    enemies[i].y = 0;
    enemies[i].speed = 1;
    enemies[i].active = true;
    return;
  }
}

void updateRoboDodge() {
  // Red = Fire
  if (pressed(BTN_RED)) {
    for (int i = 0; i < MAX_BULLETS; i++) {
      if (!bullets[i].active) {
        bullets[i].x = player.x;
        bullets[i].y = SCREEN_HEIGHT - 14;
        bullets[i].active = true;
        break;
      }
    }
    waitRelease(BTN_RED);
  }
  
  // Yellow = Left
  if (pressed(BTN_YELLOW) && player.x > 6) { 
    player.x -= 3; 
    waitRelease(BTN_YELLOW); 
  }
  
  // Blue = Right
  if (pressed(BTN_BLUE) && player.x < SCREEN_WIDTH - 6) { 
    player.x += 3; 
    waitRelease(BTN_BLUE); 
  }
  
  // White = Pause
  if (pressed(BTN_WHITE)) { 
    waitRelease(BTN_WHITE); 
    pauseMenuIndex = 0;
    gameState = STATE_PAUSE_ROBODODGE; 
    return;
  }

  // Update bullets
  for (int i = 0; i < MAX_BULLETS; i++) if (bullets[i].active) {
    bullets[i].y -= 4;
    if (bullets[i].y < 0) bullets[i].active = false;
  }

  // Spawn enemies
  if (millis() - lastEnemySpawn > 700) { 
    spawnEnemy(); 
    lastEnemySpawn = millis(); 
  }

  // Update enemies
  for (int i = 0; i < MAX_ENEMIES; i++) if (enemies[i].active) {
    enemies[i].y += enemies[i].speed;
    if (enemies[i].y > SCREEN_HEIGHT) enemies[i].active = false;

    // Check bullet collision
    for (int j = 0; j < MAX_BULLETS; j++) if (bullets[j].active) {
      if (abs(bullets[j].x - enemies[i].x) < 6 && abs(bullets[j].y - enemies[i].y) < 6) {
        enemies[i].active = false; 
        bullets[j].active = false; 
        score++;
      }
    }

    // Check player collision
    if (abs(enemies[i].x - player.x) < 6 && abs(enemies[i].y - (SCREEN_HEIGHT - 8)) < 6) {
      gameState = STATE_GAME_OVER;
    }
  }
}

void drawRoboDodge() {
  display.clearDisplay();
  
  // Score display
  display.setTextSize(1);
  display.setCursor(0, 0);
  display.print(F("Score: "));
  display.println(score);
  
  // Draw line under score
  display.drawLine(0, 10, SCREEN_WIDTH, 10, SSD1306_WHITE);

  // Draw player
  display.fillRect(player.x - 4, SCREEN_HEIGHT - 12, 8, 8, SSD1306_WHITE);
  
  // Draw bullets
  for (int i = 0; i < MAX_BULLETS; i++) if (bullets[i].active)
    display.fillRect(bullets[i].x - 1, bullets[i].y, 2, 4, SSD1306_WHITE);
  
  // Draw enemies
  for (int i = 0; i < MAX_ENEMIES; i++) if (enemies[i].active)
    display.fillCircle(enemies[i].x, enemies[i].y, 3, SSD1306_WHITE);

  display.display();
}

//  TICTACTOE 
void startTicTacToe() {
  for (int y = 0; y < GRID_SIZE; y++)
    for (int x = 0; x < GRID_SIZE; x++)
      board[y][x] = ' ';
  cursorX = 1; 
  cursorY = 1;
  xTurn = true;
  tttGameOver = false;
  tttWinner = ' ';
  tttDraw = false;
  yellowShiftActive = false;
  whiteShiftActive = false;
}

void drawTicTacToe() {
  display.clearDisplay();
  
  // Title
  display.setTextSize(1);
  display.setCursor(28, 0);
  display.print(F("Tic-Tac-Toe"));
  
  int startX = 34, startY = 14;
  int cellSize = 20;

  // Draw grid
  for (int i = 1; i < GRID_SIZE; i++) {
    display.drawLine(startX + i * cellSize, startY, startX + i * cellSize, startY + cellSize * GRID_SIZE, SSD1306_WHITE);
    display.drawLine(startX, startY + i * cellSize, startX + cellSize * GRID_SIZE, startY + i * cellSize, SSD1306_WHITE);
  }

  // Draw marks
  for (int y = 0; y < GRID_SIZE; y++) {
    for (int x = 0; x < GRID_SIZE; x++) {
      int cx = startX + x * cellSize + 6;
      int cy = startY + y * cellSize + 6;
      display.setTextSize(2);
      display.setCursor(cx, cy);
      if (board[y][x] != ' ') display.print(board[y][x]);
    }
  }

  // Highlight cursor (thicker border)
  if (!tttGameOver) {
    int highlightX = startX + cursorX * cellSize;
    int highlightY = startY + cursorY * cellSize;
    display.drawRect(highlightX, highlightY, cellSize, cellSize, SSD1306_WHITE);
    display.drawRect(highlightX + 1, highlightY + 1, cellSize - 2, cellSize - 2, SSD1306_WHITE);
  }

  // Game over message
  if (tttGameOver) {
    display.fillRect(0, 54, SCREEN_WIDTH, 10, SSD1306_BLACK);
    display.setTextSize(1);
    display.setCursor(24, 56);
    if (tttDraw) {
      display.print(F("Draw!"));
    } else {
      display.print(tttWinner);
      display.print(F(" Wins!"));
    }
  } else {
    // Show current turn
    display.setTextSize(1);
    display.setCursor(0, 56);
    display.print(F("Turn: "));
    display.print(xTurn ? 'X' : 'O');
  }

  display.display();
}

void checkTicTacToeWin() {
  // Check rows and columns
  for (int i = 0; i < GRID_SIZE; i++) {
    if (board[i][0] != ' ' && board[i][0] == board[i][1] && board[i][1] == board[i][2]) { 
      tttWinner = board[i][0]; tttGameOver = true; return; 
    }
    if (board[0][i] != ' ' && board[0][i] == board[1][i] && board[1][i] == board[2][i]) { 
      tttWinner = board[0][i]; tttGameOver = true; return; 
    }
  }
  
  // Diagonals
  if (board[0][0] != ' ' && board[0][0] == board[1][1] && board[1][1] == board[2][2]) { 
    tttWinner = board[0][0]; tttGameOver = true; return; 
  }
  if (board[0][2] != ' ' && board[0][2] == board[1][1] && board[1][1] == board[2][0]) { 
    tttWinner = board[0][2]; tttGameOver = true; return; 
  }

  // Check draw
  bool full = true;
  for (int y = 0; y < GRID_SIZE; y++)
    for (int x = 0; x < GRID_SIZE; x++)
      if (board[y][x] == ' ') full = false;

  if (full && tttWinner == ' ') { 
    tttDraw = true; 
    tttGameOver = true; 
  }
}

void updateTicTacToe() {
  if (!tttGameOver) {
    // Red = Up (always works)
    if (pressed(BTN_RED) && !pressed(BTN_WHITE) && cursorY > 0) { 
      cursorY--; 
      waitRelease(BTN_RED); 
    }
    
    // Yellow = Left (always works)
    if (pressed(BTN_YELLOW) && !pressed(BTN_BLUE) && cursorX > 0) { 
      cursorX--; 
      waitRelease(BTN_YELLOW); 
    }
    
    // White = Down OR White + Red = Pause
    if (pressed(BTN_WHITE)) {
      if (pressed(BTN_RED)) {
        // White + Red = Open pause menu
        waitRelease(BTN_WHITE);
        waitRelease(BTN_RED);
        pauseMenuIndex = 0;
        gameState = STATE_PAUSE_TICTACTOE;
        return;
      } else if (cursorY < GRID_SIZE - 1) {
        // Only White = Move down
        cursorY++;
        waitRelease(BTN_WHITE);
      } else {
        waitRelease(BTN_WHITE);
      }
    }
    
    // Blue = Right OR Yellow + Blue = Place
    if (pressed(BTN_BLUE)) {
      if (pressed(BTN_YELLOW)) {
        // Yellow + Blue = Place X or O
        if (board[cursorY][cursorX] == ' ') {
          board[cursorY][cursorX] = xTurn ? 'X' : 'O';
          xTurn = !xTurn;
          checkTicTacToeWin();
        }
        waitRelease(BTN_YELLOW);
        waitRelease(BTN_BLUE);
      } else if (cursorX < GRID_SIZE - 1) {
        // Only Blue = Move right
        cursorX++;
        waitRelease(BTN_BLUE);
      } else {
        waitRelease(BTN_BLUE);
      }
    }
  } else {
    // Game over - wait for any button to restart or go back
    if (pressed(BTN_RED) || pressed(BTN_YELLOW)) {
      waitRelease(BTN_RED);
      waitRelease(BTN_YELLOW);
      startTicTacToe();
    }
    if (pressed(BTN_WHITE) || pressed(BTN_BLUE)) {
      waitRelease(BTN_WHITE);
      waitRelease(BTN_BLUE);
      gameState = STATE_MENU;
    }
  }
}

//  PAUSE MENU
void drawPauseMenu() {
  display.clearDisplay();
  
  // Title
  display.setTextSize(2);
  display.setCursor(22, 8);
  display.println(F("PAUSED"));
  
  // Draw line separator
  display.drawLine(0, 28, SCREEN_WIDTH, 28, SSD1306_WHITE);
  
  display.setTextSize(1);
  
  // Resume option
  if (pauseMenuIndex == 0) {
    display.fillRect(20, 34, 88, 10, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  } else {
    display.setTextColor(SSD1306_WHITE);
  }
  display.setCursor(36, 36);
  display.println(F("Resume"));
  
  // Exit option
  if (pauseMenuIndex == 1) {
    display.fillRect(20, 48, 88, 10, SSD1306_WHITE);
    display.setTextColor(SSD1306_BLACK);
  } else {
    display.setTextColor(SSD1306_WHITE);
  }
  display.setCursor(42, 50);
  display.println(F("Exit"));
  
  display.setTextColor(SSD1306_WHITE);
  display.display();
}

void updatePauseMenu(bool fromRoboDodge) {
  // Red = Up
  if (pressed(BTN_RED)) {
    pauseMenuIndex = 0;
    waitRelease(BTN_RED);
  }
  // White = Down
  if (pressed(BTN_WHITE)) {
    pauseMenuIndex = 1;
    waitRelease(BTN_WHITE);
  }
  // Blue = Select
  if (pressed(BTN_BLUE)) {
    waitRelease(BTN_BLUE);
    if (pauseMenuIndex == 0) {
      // Resume
      gameState = fromRoboDodge ? STATE_ROBODODGE : STATE_TICTACTOE;
    } else {
      // Exit to main menu
      gameState = STATE_MENU;
    }
  }
}

//  LOOP 
void loop() {
  unsigned long now = millis();
  if (now - lastFrame < 33) return;
  lastFrame = now;

  switch (gameState) {
    case STATE_MENU:
      updateMenu(); 
      drawMenu(); 
      break;
      
    case STATE_ROBODODGE:
      updateRoboDodge(); 
      drawRoboDodge(); 
      break;
      
    case STATE_TICTACTOE:
      updateTicTacToe(); 
      drawTicTacToe(); 
      break;
      
    case STATE_PAUSE_ROBODODGE:
      updatePauseMenu(true);
      drawPauseMenu();
      break;
      
    case STATE_PAUSE_TICTACTOE:
      updatePauseMenu(false);
      drawPauseMenu();
      break;
      
    case STATE_GAME_OVER:
      display.clearDisplay();
      display.setTextSize(2);
      display.setCursor(8, 12);
      display.println(F("GAME OVER"));
      
      display.setTextSize(1);
      display.setCursor(30, 36);
      display.print(F("Score: "));
      display.println(score);
      
      display.setCursor(16, 52);
      display.print(F("Y=Menu W=Retry"));
      display.display();
      
      // Yellow = Back to menu
      if (pressed(BTN_YELLOW)) { 
        waitRelease(BTN_YELLOW); 
        gameState = STATE_MENU; 
      }
      // White = Retry
      if (pressed(BTN_WHITE)) { 
        waitRelease(BTN_WHITE); 
        resetRoboDodge(); 
        gameState = STATE_ROBODODGE; 
      }
      break;
  }
}
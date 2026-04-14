#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// ---- Display ----
#define SCREEN_WIDTH  128
#define SCREEN_HEIGHT  64
#define OLED_RESET     -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// ---- Buttons ----
#define BTN_LEFT  2
#define BTN_RIGHT 3
#define BTN_START 4

// ---- Road layout ----
#define ROAD_X      20
#define ROAD_WIDTH  88
#define LANE_COUNT   3
#define LANE_WIDTH  (ROAD_WIDTH / LANE_COUNT)

// ---- Player ----
#define PLAYER_W     8
#define PLAYER_H    12
#define PLAYER_Y   (SCREEN_HEIGHT - PLAYER_H - 4)

// ---- Enemies ----
#define ENEMY_W      8
#define ENEMY_H     10
#define MAX_ENEMIES  4

// ---- Game states ----
#define STATE_TITLE    0
#define STATE_PLAYING  1
#define STATE_GAMEOVER 2

// ---- Game variables ----
int playerLane = 1;
int gameState = STATE_TITLE;
int score = 0;
int highScore = 0;

// 1.5x original speed: was 8.0, now 12.0
float enemySpeed    = 12.0;
int   spawnInterval = 900;
int   scrollSpeed   = 2;
int   roadScroll    = 0;

unsigned long lastSpawn      = 0;
unsigned long lastDifficulty = 0;
unsigned long gameStartTime  = 0;
unsigned long gameEndTime    = 0;
unsigned long lastFrame      = 0;
unsigned long blinkTimer     = 0;
bool blinkOn = true;

// Button debounce
unsigned long lastLeftPress  = 0;
unsigned long lastRightPress = 0;
unsigned long lastStartPress = 0;
#define DEBOUNCE_MS 200

struct Enemy {
  int lane;
  float y;
  float speed;
  bool active;
};

Enemy enemies[MAX_ENEMIES];

int laneToX(int lane, int objWidth) {
  return ROAD_X + lane * LANE_WIDTH + (LANE_WIDTH - objWidth) / 2;
}

void drawPlayerCar(int x, int y) {
  display.drawRect(x, y, PLAYER_W, PLAYER_H, SSD1306_WHITE);
  display.fillRect(x + 2, y + 1, PLAYER_W - 4, 3, SSD1306_WHITE);
}

void drawEnemyCar(int x, int y) {
  display.drawRect(x, y, ENEMY_W, ENEMY_H, SSD1306_WHITE);
  display.fillRect(x + 2, y + ENEMY_H - 4, ENEMY_W - 4, 3, SSD1306_WHITE);
}

void drawRoad() {
  display.drawFastVLine(ROAD_X, 0, SCREEN_HEIGHT, SSD1306_WHITE);
  display.drawFastVLine(ROAD_X + ROAD_WIDTH, 0, SCREEN_HEIGHT, SSD1306_WHITE);

  int d1 = ROAD_X + LANE_WIDTH;
  int d2 = ROAD_X + 2 * LANE_WIDTH;

  for (int y = -10 + roadScroll; y < SCREEN_HEIGHT; y += 18) {
    display.drawFastVLine(d1, y, 8, SSD1306_WHITE);
    display.drawFastVLine(d2, y, 8, SSD1306_WHITE);
  }
}

void spawnEnemy() {
  bool laneBlocked[LANE_COUNT] = {false, false, false};

  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (enemies[i].active && enemies[i].y < 15) {
      laneBlocked[enemies[i].lane] = true;
    }
  }

  int available[LANE_COUNT];
  int count = 0;

  for (int i = 0; i < LANE_COUNT; i++) {
    if (!laneBlocked[i]) {
      available[count++] = i;
    }
  }

  if (count == 0) return;

  int lane = available[random(count)];

  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!enemies[i].active) {
      enemies[i].lane   = lane;
      enemies[i].y      = -ENEMY_H;
      enemies[i].speed  = enemySpeed + random(0, 2);
      enemies[i].active = true;
      return;
    }
  }
}

bool checkCollision() {
  int px = laneToX(playerLane, PLAYER_W);
  int py = PLAYER_Y;

  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!enemies[i].active) continue;

    int ex = laneToX(enemies[i].lane, ENEMY_W);
    int ey = (int)enemies[i].y;

    if (px < ex + ENEMY_W &&
        px + PLAYER_W > ex &&
        py < ey + ENEMY_H &&
        py + PLAYER_H > ey) {
      return true;
    }
  }

  return false;
}

void resetGame() {
  playerLane    = 1;
  score         = 0;

  // 1.5x original speed
  enemySpeed    = 12.0;

  spawnInterval = 900;
  scrollSpeed   = 2;
  roadScroll    = 0;

  for (int i = 0; i < MAX_ENEMIES; i++) {
    enemies[i].active = false;
  }

  lastSpawn      = millis();
  lastDifficulty = millis();
  gameStartTime  = millis();
  gameEndTime    = 0;
}

void drawTitle() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(10, 4);
  display.println("TRAFFIC WEAVER");

  display.drawRect(40, 20, 48, 30, SSD1306_WHITE);
  display.drawFastVLine(56, 20, 30, SSD1306_WHITE);
  display.drawFastVLine(72, 20, 30, SSD1306_WHITE);

  display.fillRect(43, 32, 6, 8, SSD1306_WHITE);
  display.drawRect(59, 22, 6, 8, SSD1306_WHITE);
  display.drawRect(75, 28, 6, 8, SSD1306_WHITE);

  if (blinkOn) {
    display.setCursor(22, 56);
    display.println("PRESS TO START");
  }

  display.display();
}

void drawGame() {
  display.clearDisplay();

  drawRoad();

  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (enemies[i].active) {
      drawEnemyCar(laneToX(enemies[i].lane, ENEMY_W), (int)enemies[i].y);
    }
  }

  drawPlayerCar(laneToX(playerLane, PLAYER_W), PLAYER_Y);

  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(0, 0);
  display.print("S:");
  display.print(score);

  display.setCursor(0, 10);
  display.print("H:");
  display.print(highScore);

  unsigned long elapsed = (millis() - gameStartTime) / 1000;
  display.setCursor(0, 20);
  display.print("T:");
  display.print(elapsed);

  display.display();
}

void drawGameOver() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);

  display.setCursor(28, 2);
  display.println("GAME OVER");

  display.setCursor(10, 18);
  display.print("SCORE: ");
  display.println(score);

  display.setCursor(10, 30);
  display.print("BEST:  ");
  display.println(highScore);

  unsigned long elapsed = (gameEndTime - gameStartTime) / 1000;
  display.setCursor(10, 42);
  display.print("TIME:  ");
  display.print(elapsed);
  display.println("s");

  if (blinkOn) {
    display.setCursor(16, 56);
    display.println("PRESS TO RESTART");
  }

  display.display();
}

void updateGame() {
  unsigned long now = millis();

  roadScroll = (roadScroll + scrollSpeed) % 18;

  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!enemies[i].active) continue;

    // 1.5x original movement: was 0.12f, now 0.18f
    enemies[i].y += enemies[i].speed * 0.18f;

    if (enemies[i].y > SCREEN_HEIGHT) {
      enemies[i].active = false;
      score += 10;
    }
  }

  if (now - lastSpawn >= (unsigned long)spawnInterval) {
    lastSpawn = now;
    spawnEnemy();
  }

  if (now - lastDifficulty >= 3500) {
    lastDifficulty = now;

    // 1.5x difficulty cap: original cap was 16.0, now 24.0
    if (enemySpeed < 24.0) enemySpeed += 1.5;

    if (scrollSpeed < 5) scrollSpeed++;

    if (spawnInterval > 300) spawnInterval -= 50;
  }

  if (checkCollision()) {
    if (score > highScore) highScore = score;
    gameEndTime = millis();
    gameState = STATE_GAMEOVER;
  }
}

void setup() {
  Serial.begin(9600);

  pinMode(BTN_LEFT,  INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  pinMode(BTN_START, INPUT_PULLUP);

  randomSeed(analogRead(A0));

  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("SSD1306 not found!");
    while (true);
  }

  display.clearDisplay();
  display.display();

  blinkTimer = millis();
  lastFrame  = millis();
}

void loop() {
  unsigned long now = millis();

  if (now - lastFrame < 33) return;
  lastFrame = now;

  if (now - blinkTimer > 450) {
    blinkTimer = now;
    blinkOn = !blinkOn;
  }

  bool leftPressed  = false;
  bool rightPressed = false;
  bool startPressed = false;

  if (digitalRead(BTN_LEFT) == LOW && now - lastLeftPress > DEBOUNCE_MS) {
    lastLeftPress = now;
    leftPressed = true;
  }

  if (digitalRead(BTN_RIGHT) == LOW && now - lastRightPress > DEBOUNCE_MS) {
    lastRightPress = now;
    rightPressed = true;
  }

  if (digitalRead(BTN_START) == LOW && now - lastStartPress > DEBOUNCE_MS) {
    lastStartPress = now;
    startPressed = true;
  }

  if (gameState == STATE_TITLE) {
    if (startPressed || leftPressed || rightPressed) {
      resetGame();
      gameState = STATE_PLAYING;
    }

    drawTitle();
  }

  else if (gameState == STATE_PLAYING) {
    if (leftPressed && playerLane > 0) {
      playerLane--;
    }

    if (rightPressed && playerLane < LANE_COUNT - 1) {
      playerLane++;
    }

    updateGame();
    drawGame();
  }

  else if (gameState == STATE_GAMEOVER) {
    if (startPressed || leftPressed || rightPressed) {
      resetGame();
      gameState = STATE_PLAYING;
    }

    drawGameOver();
  }
}
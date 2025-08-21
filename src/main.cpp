#include <TFT_eSPI.h>
#include <SPI.h>
#include <math.h>

TFT_eSPI tft = TFT_eSPI();

// ------------------ Constants ------------------
#define BTN_LEFT  20
#define BTN_RIGHT 21
const int PADDLE_Y = 75;
const int PADDLE_H = 5;
const int PADDLE_SPEED = 3;
const int MAX_BALLS = 3;
const int BRICK_ROWS_MAX = 6;
const int BRICK_COLS = 6;
const int BRICK_W = 24;
const int BRICK_H = 8;
const int MAX_POWERUPS = 3;
const float BALL_SPEED = 3.0;
const float SLOW_FACTOR = 0.6;

// ------------------ Game Objects ------------------
struct Ball {
  float x, y, vx, vy;
  bool active;
};

struct PowerUp {
  float x, y;
  int type; // 0=paddle+long, 1=multi-ball, 2=slow, 3=life
  bool active;
};

int paddleX = 60, paddleW = 30;
unsigned long paddlePowerEnd = 0;
Ball balls[MAX_BALLS];
bool bricks[BRICK_ROWS_MAX][BRICK_COLS];
int brickRows = 3;
PowerUp powerups[MAX_POWERUPS];
int score = 0, level = 1, lives = 3;
bool isGameOver = false;
unsigned long lastBtnPress = 0;
float ballSpeed = BALL_SPEED;

// ------------------ Previous Positions (Static Arrays) ------------------
int prevPaddleX = paddleX;
float prevBallX[MAX_BALLS], prevBallY[MAX_BALLS];
float prevPowerX[MAX_POWERUPS], prevPowerY[MAX_POWERUPS];

// ------------------ Setup -----------------------
void setup() {
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);
  tft.init();
  tft.setRotation(3);
  tft.fillScreen(TFT_BLACK);
  randomSeed(analogRead(0));
  resetGameFull();
}

// ------------------ Main Loop (Time-based) ------------------
unsigned long lastUpdate = 0;
void loop() {
  unsigned long now = millis();
  if (now - lastUpdate < 10) return; // Target ~100 FPS
  lastUpdate = now;

  if (isGameOver) {
    if ((digitalRead(BTN_LEFT) == LOW || digitalRead(BTN_RIGHT) == LOW) && now - lastBtnPress > 200) {
      score = 0; level = 1; brickRows = 3; lives = 3; isGameOver = false;
      resetGameFull();
      lastBtnPress = now;
    }
    return;
  }

  handlePaddle();
  updateBalls();
  updatePowerUps();
  drawHUD();
}

// ------------------ Paddle ----------------------
void handlePaddle() {
  int newPaddleX = paddleX;
  if (millis() - lastBtnPress > 50) { // Debounce 50ms
    if (digitalRead(BTN_LEFT) == LOW && paddleX > 0) newPaddleX -= PADDLE_SPEED;
    if (digitalRead(BTN_RIGHT) == LOW && paddleX + paddleW < 160) newPaddleX += PADDLE_SPEED;
    lastBtnPress = millis();
  }

  if (paddlePowerEnd > 0 && millis() > paddlePowerEnd) {
    paddleW = 30;
    ballSpeed = BALL_SPEED; // Reset ball speed
    paddlePowerEnd = 0;
  }

  if (newPaddleX != paddleX) {
    tft.fillRect(paddleX, PADDLE_Y, paddleW, PADDLE_H, TFT_BLACK);
    paddleX = newPaddleX;
    drawPaddle();
  }
}

void drawPaddle() {
  tft.fillRect(paddleX, PADDLE_Y, paddleW, PADDLE_H, TFT_WHITE);
}

// ------------------ Ball ----------------------
void initBall(int idx, float x, float y, float vx, float vy) {
  balls[idx] = {x, y, vx, vy, true};
  prevBallX[idx] = x; prevBallY[idx] = y;
}

void updateBalls() {
  for (int i = 0; i < MAX_BALLS; i++) {
    if (!balls[i].active) continue;

    tft.fillRect(prevBallX[i], prevBallY[i], 4, 4, TFT_BLACK);
    balls[i].x += balls[i].vx;
    balls[i].y += balls[i].vy;

    // Wall collision
    if (balls[i].x <= 0) { balls[i].x = 0; balls[i].vx = fabs(balls[i].vx); }
    if (balls[i].x + 4 >= 160) { balls[i].x = 156; balls[i].vx = -fabs(balls[i].vx); }
    if (balls[i].y <= 8) { balls[i].y = 8; balls[i].vy = -balls[i].vy; }

    // Paddle collision
    if (balls[i].y + 4 >= PADDLE_Y && balls[i].x + 4 >= paddleX && balls[i].x <= paddleX + paddleW) {
      balls[i].vy = -fabs(balls[i].vy);
      float hitPos = (balls[i].x + 2) - (paddleX + paddleW / 2);
      balls[i].vx = hitPos * 0.15;
      float len = sqrt(balls[i].vx * balls[i].vx + balls[i].vy * balls[i].vy);
      if (len > 0) {
        balls[i].vx = (balls[i].vx / len) * ballSpeed;
        balls[i].vy = (balls[i].vy / len) * ballSpeed;
      }
      balls[i].y = PADDLE_Y - 4;
    }

    // Brick collision
    int row = (int)(balls[i].y / BRICK_H);
    int col = (int)(balls[i].x / BRICK_W);
    if (row >= 0 && row < brickRows && col >= 0 && col < BRICK_COLS && bricks[row][col]) {
      bricks[row][col] = false;
      tft.fillRect(col * BRICK_W + 1, row * BRICK_H + 1, BRICK_W - 2, BRICK_H - 2, TFT_BLACK);
      balls[i].vy = -balls[i].vy;
      score += 10;
      if (random(100) < 20) spawnPowerUp(col * BRICK_W + BRICK_W / 2, row * BRICK_H + BRICK_H / 2);
      if (allBricksCleared()) { level++; brickRows = min(brickRows + 1, BRICK_ROWS_MAX); delay(300); resetGameFull(); return; }
    }

    // Game over check
    if (balls[i].y + 4 >= 80) {
      balls[i].active = false;
      checkGameOver();
      continue;
    }

    drawBall(i);
    prevBallX[i] = balls[i].x;
    prevBallY[i] = balls[i].y;
  }
}

void drawBall(int idx) {
  tft.fillRect(balls[idx].x, balls[idx].y, 4, 4, TFT_RED);
}

bool allBricksCleared() {
  for (int r = 0; r < brickRows; r++)
    for (int c = 0; c < BRICK_COLS; c++)
      if (bricks[r][c]) return false;
  return true;
}

void checkGameOver() {
  for (int i = 0; i < MAX_BALLS; i++)
    if (balls[i].active) return;
  lives--;
  if (lives > 0) resetBallPaddlePreserveBricks();
  else showGameOver();
}

// ------------------ Power-up --------------------
void spawnPowerUp(float x, float y) {
  for (int i = 0; i < MAX_POWERUPS; i++) {
    if (!powerups[i].active) {
      powerups[i] = {x, y, random(4), true};
      prevPowerX[i] = x; prevPowerY[i] = y;
      break;
    }
  }
}

void updatePowerUps() {
  for (int i = 0; i < MAX_POWERUPS; i++) {
    if (!powerups[i].active) continue;
    tft.fillRect(prevPowerX[i], prevPowerY[i], 6, 6, TFT_BLACK);
    powerups[i].y += 1;

    // Paddle catch
    if (powerups[i].y + 6 >= PADDLE_Y && powerups[i].x + 3 >= paddleX && powerups[i].x - 3 <= paddleX + paddleW) {
      activatePowerUp(powerups[i].type);
      powerups[i].active = false;
      continue;
    }

    // Fall off
    if (powerups[i].y > 80) powerups[i].active = false;

    // Draw power-up
    tft.fillRect(powerups[i].x, powerups[i].y, 6, 6, (powerups[i].type == 0 ? TFT_BLUE : powerups[i].type == 1 ? TFT_YELLOW : powerups[i].type == 2 ? TFT_GREEN : TFT_RED));
    prevPowerX[i] = powerups[i].x;
    prevPowerY[i] = powerups[i].y;
  }
}

void activatePowerUp(int type) {
  switch (type) {
    case 0: // Paddle long
      paddleW = 50;
      paddlePowerEnd = millis() + 10000;
      break;
    case 1: // Multi-ball
      for (int i = 0; i < MAX_BALLS; i++)
        if (!balls[i].active) {
          initBall(i, balls[0].x, balls[0].y, ballSpeed * cos(random(0, 360) * 3.14 / 180.0), -ballSpeed * sin(random(0, 360) * 3.14 / 180.0));
          break;
        }
      break;
    case 2: // Slow
      ballSpeed = BALL_SPEED * SLOW_FACTOR;
      paddlePowerEnd = millis() + 8000;
      break;
    case 3: // Life
      lives++;
      break;
  }
}

// ------------------ Reset / Game Over ------------------
void resetGameFull() {
  tft.fillScreen(TFT_BLACK);
  paddleX = 60; paddleW = 30; drawPaddle();
  for (int i = 0; i < MAX_BALLS; i++) balls[i].active = false;
  initBall(0, 80, 40, 2.0, -2.0);
  for (int r = 0; r < BRICK_ROWS_MAX; r++)
    for (int c = 0; c < BRICK_COLS; c++)
      if (r < brickRows) { bricks[r][c] = true; tft.fillRect(c * BRICK_W + 1, r * BRICK_H + 1, BRICK_W - 2, BRICK_H - 2, r % 2 == 0 ? TFT_GREEN : TFT_CYAN); }
      else bricks[r][c] = false;
  for (int i = 0; i < MAX_POWERUPS; i++) powerups[i].active = false;
  drawHUD();
}

void resetBallPaddlePreserveBricks() {
  paddleX = 60; paddleW = 30; drawPaddle();
  for (int i = 0; i < MAX_BALLS; i++) balls[i].active = false;
  initBall(0, 80, 40, 2.0, -2.0);
}

void drawHUD() {
  tft.fillRect(0, 0, 160, 8, TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(2, 0); tft.print("Sc:"); tft.print(score);
  tft.setCursor(70, 0); tft.print("Lv:"); tft.print(level);
  tft.setCursor(110, 0); tft.print("L:"); tft.print(lives);
}

void showGameOver() {
  isGameOver = true;
  tft.fillRect(30, 28, 100, 24, TFT_WHITE);
  tft.drawRect(30, 28, 100, 24, TFT_BLACK);
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.setCursor(45, 34); tft.print("GAME OVER");
  tft.setCursor(38, 44); tft.print("Press Btn to restart");
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
}
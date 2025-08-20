#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI( );

// Nút điều khiển
#define BTN_LEFT  0
#define BTN_RIGHT 2

// Paddle
int paddleX = 60;
int paddleW = 30;
const int paddleY = 70;
const int paddleH = 5;

// Ball
int ballX, ballY;
int ballDX, ballDY;
const int ballR = 3;

// Blocks
const int rowsMax = 5, cols = 6;
bool blocks[rowsMax][cols];
const int blockW = 25, blockH = 8;

// Game state
int score = 0;
int lives = 3;
int level = 1;
int speedDelay = 15;

// Power-ups
struct PowerUp {
  int x, y;
  int type;    // 1=to paddle, 2=thu nhỏ paddle, 3=tăng tốc, 4=thêm mạng
  bool active;
};
PowerUp powerups[5];  // tối đa 5 vật phẩm rơi cùng lúc

void spawnPowerUp(int bx, int by) {
  for (int i = 0; i < 5; i++) {
    if (!powerups[i].active) {
      powerups[i].x = bx;
      powerups[i].y = by;
      powerups[i].type = random(1, 5); // 1..4
      powerups[i].active = true;
      break;
    }
  }
}

void drawPowerUps() {
  for (int i = 0; i < 5; i++) {
    if (powerups[i].active) {
      uint16_t color;
      switch (powerups[i].type) {
        case 1: color = TFT_GREEN; break;
        case 2: color = TFT_ORANGE; break;
        case 3: color = TFT_BLUE; break;
        case 4: color = TFT_MAGENTA; break;
      }
      tft.fillRect(powerups[i].x, powerups[i].y, 8, 8, color);
    }
  }
}

void updatePowerUps() {
  for (int i = 0; i < 5; i++) {
    if (powerups[i].active) {
      powerups[i].y += 1; // rơi xuống
      if (powerups[i].y >= paddleY && 
          powerups[i].x >= paddleX && powerups[i].x <= paddleX + paddleW) {
        // Paddle bắt được
        switch (powerups[i].type) {
          case 1: paddleW += 10; if (paddleW > 60) paddleW = 60; break;
          case 2: paddleW -= 8; if (paddleW < 15) paddleW = 15; break;
          case 3: if (speedDelay > 5) speedDelay -= 3; break;
          case 4: lives++; break;
        }
        powerups[i].active = false;
      }
      if (powerups[i].y > 80) powerups[i].active = false; // rơi ra ngoài
    }
  }
}

void drawPaddle() {
  tft.fillRect(paddleX, paddleY, paddleW, paddleH, TFT_WHITE);
}

void drawBall() {
  tft.fillCircle(ballX, ballY, ballR, TFT_YELLOW);
}

void drawBlocks() {
  for (int r = 0; r < rowsMax; r++) {
    for (int c = 0; c < cols; c++) {
      if (blocks[r][c]) {
        int bx = c * blockW + 5;
        int by = r * (blockH + 2) + 10;
        uint16_t color = (r % 2 == 0) ? TFT_RED : TFT_CYAN;
        tft.fillRect(bx, by, blockW - 2, blockH, color);
      }
    }
  }
}

void drawHUD() {
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(2, 0);
  tft.printf("Lv:%d  Sc:%d  L:%d", level, score, lives);
}

void initBlocks() {
  for (int r = 0; r < level + 1 && r < rowsMax; r++) {
    for (int c = 0; c < cols; c++) {
      blocks[r][c] = true;
    }
  }
}

bool allBlocksCleared() {
  for (int r = 0; r < rowsMax; r++) {
    for (int c = 0; c < cols; c++) {
      if (blocks[r][c]) return false;
    }
  }
  return true;
}

void resetBallPaddle() {
  paddleX = 60;
  paddleW = 30;
  ballX = 80;
  ballY = 50;
  ballDX = (random(0, 2) == 0) ? 1 : -1;
  ballDY = -1;
  for (int i = 0; i < 5; i++) powerups[i].active = false;
}

void newGame() {
  score = 0;
  lives = 3;
  level = 1;
  speedDelay = 15;
  initBlocks();
  resetBallPaddle();
}

void setup() {
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);

  randomSeed(analogRead(0)); // để random powerup
  newGame();
}

void loop() {
  // Xóa màn hình
  tft.fillScreen(TFT_BLACK);

  // Điều khiển paddle
  if (!digitalRead(BTN_LEFT) && paddleX > 0) paddleX -= 2;
  if (!digitalRead(BTN_RIGHT) && paddleX < (160 - paddleW)) paddleX += 2;

  // Cập nhật bóng
  ballX += ballDX;
  ballY += ballDY;

  // Va chạm tường
  if (ballX - ballR <= 0 || ballX + ballR >= 160) ballDX = -ballDX;
  if (ballY - ballR <= 8) ballDY = -ballDY;

  // Va chạm paddle
  if (ballY + ballR >= paddleY && ballX >= paddleX && ballX <= paddleX + paddleW) {
    ballDY = -ballDY;
    ballY = paddleY - ballR;
  }

  // Va chạm blocks
  for (int r = 0; r < rowsMax; r++) {
    for (int c = 0; c < cols; c++) {
      if (blocks[r][c]) {
        int bx = c * blockW + 5;
        int by = r * (blockH + 2) + 10;
        if (ballX >= bx && ballX <= bx + blockW &&
            ballY >= by && ballY <= by + blockH) {
          blocks[r][c] = false;
          ballDY = -ballDY;
          score += 10;
          if (random(0, 100) < 20) { // 20% ra powerup
            spawnPowerUp(bx + blockW / 2, by);
          }
        }
      }
    }
  }

  // Nếu bóng rơi xuống đáy
  if (ballY > 80) {
    lives--;
    if (lives > 0) {
      resetBallPaddle();
    } else {
      // Game Over
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.setTextSize(2);
      tft.setCursor(30, 30);
      tft.println("GAME OVER");
      tft.setTextSize(1);
      tft.setCursor(20, 60);
      tft.printf("Score: %d", score);
      delay(3000);
      newGame();
    }
  }

  // Nếu qua màn
  if (allBlocksCleared()) {
    level++;
    if (speedDelay > 5) speedDelay -= 2;
    initBlocks();
    resetBallPaddle();
  }

  // Vẽ
  drawBlocks();
  drawPaddle();
  drawBall();
  drawHUD();
  drawPowerUps();

  // Update powerup
  updatePowerUps();

  delay(speedDelay);
}
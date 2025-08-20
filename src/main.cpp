#include <Arduino.h>
#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <SPI.h>

// Include các file video
#include "video01.h"
#include "video05.h"
#include "video06.h"
#include "video10.h"
#include "video11.h"
#include "video12.h"
#include "video13.h"
// Loại bỏ "video14.h" vì không có trong include gốc và gây lỗi

// Định nghĩa chân nút bấm
#define BUTTON_SELECT 0   // Chuyển đổi tùy chọn menu
#define BUTTON_CONFIRM 2  // Chọn dự án / Nhảy (Flappy Bird) / Di chuyển (Breakout)

// Khởi tạo TFT
TFT_eSPI tft = TFT_eSPI();

// Biến menu
int currentOption = 0;  // 0: Video Player, 1: Flappy Bird, 2: Breakout
bool projectRunning = false;
unsigned long lastDebounceTime = 0;
const unsigned long debounceDelay = 50;
unsigned long confirmPressTime = 0;  // Thời gian nhấn nút xác nhận
const unsigned long holdToExit = 3000;  // Nhấn giữ 3s để thoát

// === Dự án 1: Video Player ===
typedef struct _VideoInfo {
  const uint8_t* const* frames;
  const uint16_t* frames_size;
  uint16_t num_frames;
} VideoInfo;

VideoInfo* videoList[] = {
  &video01,
  &video05,
  &video06,
  &video10,
  &video11,
  &video12,
  &video13
};
const uint8_t NUM_VIDEOS = sizeof(videoList) / sizeof(videoList[0]);

bool tft_output(int16_t x, int16_t y, uint16_t w, uint16_t h, uint16_t* bitmap) {
  if (x >= tft.width() || y >= tft.height()) return false;
  tft.pushImage(x, y, w, h, bitmap);
  return true;
}

void drawJPEGFrame(const VideoInfo* video, uint16_t frameIndex) {
  const uint8_t* jpg_data = (const uint8_t*)pgm_read_ptr(&video->frames[frameIndex]);
  uint16_t jpg_size = pgm_read_word(&video->frames_size[frameIndex]);
  if (!TJpgDec.drawJpg(0, 0, jpg_data, jpg_size)) {
    Serial.printf("❌ Decode failed on frame %d\n", frameIndex);
  }
}

void runVideoPlayer() {
  while (projectRunning) { // Giữ nguyên logic chạy tuần tự, thêm thoát
    for (uint8_t v = 0; v < NUM_VIDEOS; v++) {
      VideoInfo* currentVideo = videoList[v];
      for (uint16_t f = 0; f < currentVideo->num_frames; f++) {
        if (digitalRead(BUTTON_CONFIRM) == LOW) {
          if (millis() - confirmPressTime > debounceDelay) {
            confirmPressTime = millis();
          }
          if (millis() - confirmPressTime >= holdToExit) {
            projectRunning = false;
            tft.fillScreen(TFT_BLACK);
            return;
          }
        } else {
          confirmPressTime = 0;
        }
        drawJPEGFrame(currentVideo, f);
        delay(20);  // Giữ nguyên thời gian giữa các frame
      }
      delay(300);  // Giữ nguyên delay giữa các video
    }
  }
}

// === Dự án 2: Flappy Bird ===
#define SCREEN_W 160
#define SCREEN_H 80
#define GROUND_H 10
#define PIPE_W 20
#define GAP_H 30
#define GRAVITY 0.18
#define JUMP_VEL -2.0

float birdY = 25, birdVel = 0;
int birdX = 30;
int pipeX = SCREEN_W, pipeGapY = 30;
int scoreFlappy = 0;
bool gameOverFlappy = false;

TFT_eSprite sprBird = TFT_eSprite(&tft);
TFT_eSprite sprPipe = TFT_eSprite(&tft);
TFT_eSprite sprGround = TFT_eSprite(&tft);

int prevBirdX, prevBirdY;
int prevPipeX, prevPipeGapY;
bool prevGameOverFlappy = false;

unsigned long lastFrameFlappy = 0;
const int FRAME_TIME = 32;

void createBirdSprite() {
  sprBird.createSprite(8, 8);
  sprBird.fillSprite(TFT_TRANSPARENT);
  sprBird.fillCircle(4, 4, 4, TFT_YELLOW);
  sprBird.fillCircle(2, 3, 1, TFT_BLACK);
  sprBird.fillRect(6, 4, 2, 2, TFT_ORANGE);
}

void createPipeSprite() {
  sprPipe.createSprite(PIPE_W, SCREEN_H);
  sprPipe.fillSprite(TFT_GREEN);
  sprPipe.drawRect(0, 0, PIPE_W, SCREEN_H, TFT_DARKGREEN);
}

void createGroundSprite() {
  sprGround.createSprite(SCREEN_W, GROUND_H);
  sprGround.fillSprite(TFT_BROWN);
  for (int i = 0; i < SCREEN_W; i += 4) {
    sprGround.fillRect(i, GROUND_H - 2, 2, 2, TFT_DARKGREY);
  }
}

void resetFlappyGame() {
  birdY = 40;
  birdVel = 0;
  pipeX = SCREEN_W;
  pipeGapY = random(10, SCREEN_H - GAP_H - GROUND_H - 10);
  scoreFlappy = 0;
  gameOverFlappy = false;
}

void runFlappyBird() {
  unsigned long now = millis();
  if (now - lastFrameFlappy < FRAME_TIME) return;
  lastFrameFlappy = now;

  if (digitalRead(BUTTON_CONFIRM) == LOW) {
    if (millis() - confirmPressTime > debounceDelay) {
      confirmPressTime = millis();
    }
    if (millis() - confirmPressTime >= holdToExit) {
      projectRunning = false;
      tft.fillScreen(TFT_BLACK);
      return;
    }
  } else {
    confirmPressTime = 0;
  }

  static unsigned long lastBtn = 0;
  if (digitalRead(BUTTON_CONFIRM) == LOW && millis() - lastBtn > 150) {
    birdVel = JUMP_VEL;
    lastBtn = millis();
  }

  if (!gameOverFlappy) {
    birdVel += GRAVITY;
    birdY += birdVel;
    pipeX -= 2;
    if (pipeX < -PIPE_W) {
      pipeX = SCREEN_W;
      pipeGapY = random(10, SCREEN_H - GAP_H - GROUND_H - 10);
      scoreFlappy++;
    }
    if (birdY <= 0 || birdY + 8 >= SCREEN_H - GROUND_H ||
        (birdX + 8 > pipeX && birdX < pipeX + PIPE_W &&
         (birdY < pipeGapY || birdY + 8 > pipeGapY + GAP_H))) {
      gameOverFlappy = true;
    }
  } else {
    if (digitalRead(BUTTON_CONFIRM) == LOW && millis() - lastBtn > 150) {
      resetFlappyGame();
      lastBtn = millis();
    }
  }

  tft.fillRect(prevBirdX, prevBirdY, 8, 8, TFT_CYAN);
  tft.fillRect(prevPipeX, 0, PIPE_W, prevPipeGapY, TFT_CYAN);
  tft.fillRect(prevPipeX, prevPipeGapY + GAP_H, PIPE_W, SCREEN_H - (prevPipeGapY + GAP_H + GROUND_H), TFT_CYAN);
  if (prevGameOverFlappy) {
    tft.fillRect(30, 30, 100, 20, TFT_CYAN);
  }

  sprPipe.pushSprite(pipeX, 0, 0, 0, PIPE_W, pipeGapY);
  sprPipe.pushSprite(pipeX, pipeGapY + GAP_H, 0, pipeGapY + GAP_H, PIPE_W, SCREEN_H - (prevPipeGapY + GAP_H + GROUND_H));
  sprBird.pushSprite(birdX, birdY, TFT_TRANSPARENT);
  sprGround.pushSprite(0, SCREEN_H - GROUND_H);
  tft.fillRect(0, 0, 60, 10, TFT_CYAN);
  tft.setTextColor(TFT_BLACK);
  tft.setCursor(5, 2);
  tft.print("Score: ");
  tft.print(scoreFlappy);
  if (gameOverFlappy) {
    tft.fillRect(30, 30, 100, 20, TFT_WHITE);
    tft.drawRect(30, 30, 100, 20, TFT_BLACK);
    tft.setCursor(50, 37);
    tft.print("GAME OVER");
  }

  prevBirdX = birdX;
  prevBirdY = birdY;
  prevPipeX = pipeX;
  prevPipeGapY = pipeGapY;
  prevGameOverFlappy = gameOverFlappy;
}

// === Dự án 3: Breakout ===
int paddleX = 60, paddleW = 30;
const int paddleY = 70, paddleH = 5;
int ballX, ballY, ballDX, ballDY;
const int ballR = 3;
const int rowsMax = 5, cols = 6;
bool blocks[rowsMax][cols];
const int blockW = 25, blockH = 8;
int scoreBreakout = 0, lives = 3, level = 1, speedDelay = 15;

struct PowerUp {
  int x, y, type;
  bool active;
};
PowerUp powerups[5];

void spawnPowerUp(int bx, int by) {
  for (int i = 0; i < 5; i++) {
    if (!powerups[i].active) {
      powerups[i].x = bx;
      powerups[i].y = by;
      powerups[i].type = random(1, 5);
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
      powerups[i].y += 1;
      if (powerups[i].y >= paddleY && powerups[i].x >= paddleX && powerups[i].x <= paddleX + paddleW) {
        switch (powerups[i].type) {
          case 1: paddleW += 10; if (paddleW > 60) paddleW = 60; break;
          case 2: paddleW -= 8; if (paddleW < 15) paddleW = 15; break;
          case 3: if (speedDelay > 5) speedDelay -= 3; break;
          case 4: lives++; break;
        }
        powerups[i].active = false;
      }
      if (powerups[i].y > 80) powerups[i].active = false;
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
  tft.printf("Lv:%d  Sc:%d  L:%d", level, scoreBreakout, lives);
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

void newBreakoutGame() {
  scoreBreakout = 0;
  lives = 3;
  level = 1;
  speedDelay = 15;
  initBlocks();
  resetBallPaddle();
}

void runBreakout() {
  if (digitalRead(BUTTON_CONFIRM) == LOW) {
    if (millis() - confirmPressTime > debounceDelay) {
      confirmPressTime = millis();
    }
    if (millis() - confirmPressTime >= holdToExit) {
      projectRunning = false;
      tft.fillScreen(TFT_BLACK);
      return;
    }
  } else {
    confirmPressTime = 0;
  }

  tft.fillScreen(TFT_BLACK);
  static unsigned long lastBtn = 0;
  if (digitalRead(BUTTON_SELECT) == LOW && paddleX > 0 && millis() - lastBtn > 50) {
    paddleX -= 2;
    lastBtn = millis();
  }
  if (digitalRead(BUTTON_CONFIRM) == LOW && paddleX < (160 - paddleW) && millis() - lastBtn > 50) {
    paddleX += 2;
    lastBtn = millis();
  }

  ballX += ballDX;
  ballY += ballDY;

  if (ballX - ballR <= 0 || ballX + ballR >= 160) ballDX = -ballDX;
  if (ballY - ballR <= 8) ballDY = -ballDY;

  if (ballY + ballR >= paddleY && ballX >= paddleX && ballX <= paddleX + paddleW) {
    ballDY = -ballDY;
    ballY = paddleY - ballR;
  }

  for (int r = 0; r < rowsMax; r++) {
    for (int c = 0; c < cols; c++) {
      if (blocks[r][c]) {
        int bx = c * blockW + 5;
        int by = r * (blockH + 2) + 10;
        if (ballX >= bx && ballX <= bx + blockW && ballY >= by && ballY <= by + blockH) {
          blocks[r][c] = false;
          ballDY = -ballDY;
          scoreBreakout += 10;
          if (random(0, 100) < 20) {
            spawnPowerUp(bx + blockW / 2, by);
          }
        }
      }
    }
  }

  if (ballY > 80) {
    lives--;
    if (lives > 0) {
      resetBallPaddle();
    } else {
      tft.fillScreen(TFT_BLACK);
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.setTextSize(2);
      tft.setCursor(30, 30);
      tft.println("GAME OVER");
      tft.setTextSize(1);
      tft.setCursor(20, 60);
      tft.printf("Score: %d", scoreBreakout);
      delay(3000);
      newBreakoutGame();
    }
  }

  if (allBlocksCleared()) {
    level++;
    if (speedDelay > 5) speedDelay -= 2;
    initBlocks();
    resetBallPaddle();
  }

  drawBlocks();
  drawPaddle();
  drawBall();
  drawHUD();
  drawPowerUps();
  updatePowerUps();

  delay(speedDelay);
}

// === Menu ===
void displayMenu() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(10, 10);
  tft.println("=== MENU ===");
  tft.setCursor(10, 30);
  tft.print("Select: ");
  switch (currentOption) {
    case 0: tft.println("Video Player"); break;
    case 1: tft.println("Flappy Bird"); break;
    case 2: tft.println("Breakout"); break;
  }
  tft.setCursor(10, 50);
  tft.println("BTN0: Change, BTN2: Select");
}

void setup() {
  Serial.begin(115200);
  tft.begin();
  tft.setRotation(3);  // Đổi về giống code gốc
  tft.fillScreen(TFT_BLACK);

  pinMode(BUTTON_SELECT, INPUT_PULLUP);
  pinMode(BUTTON_CONFIRM, INPUT_PULLUP);

  TJpgDec.setJpgScale(1);
  TJpgDec.setSwapBytes(true);
  TJpgDec.setCallback(tft_output);

  createBirdSprite();
  createPipeSprite();
  createGroundSprite();
  resetFlappyGame();
  newBreakoutGame();

  displayMenu();
}

void loop() {
  if (projectRunning) {
    switch (currentOption) {
      case 0: runVideoPlayer(); break;
      case 1: runFlappyBird(); break;
      case 2: runBreakout(); break;
    }
    if (!projectRunning) {
      displayMenu();
    }
    return;
  }

  static int lastButtonSelectState = HIGH;
  int buttonSelectState = digitalRead(BUTTON_SELECT);
  if (buttonSelectState != lastButtonSelectState && millis() - lastDebounceTime > debounceDelay) {
    if (buttonSelectState == LOW) {
      currentOption = (currentOption + 1) % 3;
      displayMenu();
      lastDebounceTime = millis();
    }
  }
  lastButtonSelectState = buttonSelectState;

  static int lastButtonConfirmState = HIGH;
  int buttonConfirmState = digitalRead(BUTTON_CONFIRM);
  if (buttonConfirmState != lastButtonConfirmState && millis() - lastDebounceTime > debounceDelay) {
    if (buttonConfirmState == LOW) {
      projectRunning = true;
      tft.fillScreen(TFT_BLACK);
      confirmPressTime = millis();
    }
    lastDebounceTime = millis();
  }
  lastButtonConfirmState = buttonConfirmState;
}
#include <Arduino.h>
#include <TFT_eSPI.h> // Thư viện đã cấu hình chân trong User_Setup.h

#define BTN_JUMP 23 // Nút nhảy

TFT_eSPI tft = TFT_eSPI();

// ==== Cấu hình game ====
#define SCREEN_W 160
#define SCREEN_H 80
#define GROUND_H 10
#define PIPE_W   20
#define GAP_H    30
#define GRAVITY  0.5
#define JUMP_VEL -3.5

// Bird
float birdY = 40;
float birdVel = 0;
int birdX = 30;

// Pipe
int pipeX = SCREEN_W;
int pipeGapY = 30;

// Score
int score = 0;

// Timing
unsigned long lastFrame = 0;
const int FRAME_TIME = 16; // ~60 FPS

bool gameOver = false;

void drawBird(int x, int y, uint16_t color) {
  tft.fillRect(x, y, 6, 6, color);
}

void drawPipe(int x, int gapY, uint16_t color) {
  tft.fillRect(x, 0, PIPE_W, gapY, color); // Trên
  tft.fillRect(x, gapY + GAP_H, PIPE_W, SCREEN_H - (gapY + GAP_H + GROUND_H), color); // Dưới
}

void resetGame() {
  birdY = 40;
  birdVel = 0;
  pipeX = SCREEN_W;
  pipeGapY = random(10, SCREEN_H - GAP_H - GROUND_H - 10);
  score = 0;
  gameOver = false;
}

void setup() {
  pinMode(BTN_JUMP, INPUT_PULLUP);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_CYAN);
  randomSeed(analogRead(0));

  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(20, 35);
  tft.print("Flappy Bird ESP32");
  delay(1000);

  resetGame();
}

void loop() {
  unsigned long now = millis();
  if (now - lastFrame < FRAME_TIME) return;
  lastFrame = now;

  // ===== Input =====
  static unsigned long lastBtn = 0;
  if (digitalRead(BTN_JUMP) == LOW && millis() - lastBtn > 150) {
    birdVel = JUMP_VEL;
    lastBtn = millis();
  }

  // ===== Update =====
  if (!gameOver) {
    birdVel += GRAVITY;
    birdY += birdVel;

    pipeX -= 2;
    if (pipeX < -PIPE_W) {
      pipeX = SCREEN_W;
      pipeGapY = random(10, SCREEN_H - GAP_H - GROUND_H - 10);
      score++;
    }

    // Va chạm
    if (birdY <= 0 || birdY + 6 >= SCREEN_H - GROUND_H ||
        (birdX + 6 > pipeX && birdX < pipeX + PIPE_W &&
         (birdY < pipeGapY || birdY + 6 > pipeGapY + GAP_H))) {
      gameOver = true;
    }
  } else {
    if (digitalRead(BTN_JUMP) == LOW) {
      resetGame();
    }
  }

  // ===== Draw =====
  tft.fillScreen(TFT_CYAN); // nền trời
  drawPipe(pipeX, pipeGapY, TFT_GREEN);
  drawBird(birdX, birdY, TFT_YELLOW);
  tft.fillRect(0, SCREEN_H - GROUND_H, SCREEN_W, GROUND_H, TFT_BROWN);

  tft.setTextColor(TFT_BLACK);
  tft.setCursor(5, 2);
  tft.print("Score: ");
  tft.print(score);

  if (gameOver) {
    tft.setCursor(40, 35);
    tft.print("GAME OVER");
  }
}
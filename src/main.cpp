#include <Arduino.h>
#include <TFT_eSPI.h> // Đã cấu hình chân trong User_Setup.h

#define BTN_JUMP 23 // Nút nhảy

TFT_eSPI tft = TFT_eSPI();

// ==== Game config ====
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
bool gameOver = false;

// Sprite
TFT_eSprite sprBird = TFT_eSprite(&tft);
TFT_eSprite sprPipe = TFT_eSprite(&tft);
TFT_eSprite sprGround = TFT_eSprite(&tft);

unsigned long lastFrame = 0;
const int FRAME_TIME = 12; // ~80 FPS

// ===== Vẽ chim (nhiều màu) =====
void createBirdSprite() {
  sprBird.createSprite(8, 8);
  sprBird.fillSprite(TFT_TRANSPARENT);
  sprBird.fillCircle(4, 4, 4, TFT_YELLOW);  // Thân
  sprBird.fillCircle(2, 3, 1, TFT_BLACK);   // Mắt
  sprBird.fillRect(6, 4, 2, 2, TFT_ORANGE); // Mỏ
}

// ===== Vẽ ống =====
void createPipeSprite() {
  sprPipe.createSprite(PIPE_W, SCREEN_H);
  sprPipe.fillSprite(TFT_TRANSPARENT);
  sprPipe.fillRect(0, 0, PIPE_W, SCREEN_H, TFT_GREEN);
  sprPipe.drawRect(0, 0, PIPE_W, SCREEN_H, TFT_DARKGREEN);
}

// ===== Vẽ nền đất =====
void createGroundSprite() {
  sprGround.createSprite(SCREEN_W, GROUND_H);
  sprGround.fillSprite(TFT_BROWN);
  for (int i = 0; i < SCREEN_W; i += 4) {
    sprGround.fillRect(i, GROUND_H - 2, 2, 2, TFT_DARKGREY);
  }
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

  // Tạo sprite
  createBirdSprite();
  createPipeSprite();
  createGroundSprite();

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
    if (birdY <= 0 || birdY + 8 >= SCREEN_H - GROUND_H ||
        (birdX + 8 > pipeX && birdX < pipeX + PIPE_W &&
         (birdY < pipeGapY || birdY + 8 > pipeGapY + GAP_H))) {
      gameOver = true;
    }
  } else {
    if (digitalRead(BTN_JUMP) == LOW) {
      resetGame();
    }
  }

  // ===== Draw Partial Redraw =====
  // Vẽ nền trời khu vực ống và chim
  tft.fillRect(pipeX, 0, PIPE_W, SCREEN_H - GROUND_H, TFT_CYAN);
  tft.fillRect(birdX, (int)birdY - 1, 10, 10, TFT_CYAN);

  // Vẽ ống
  sprPipe.pushSprite(pipeX, pipeGapY + GAP_H, 0, pipeGapY + GAP_H,
                     PIPE_W, SCREEN_H - (pipeGapY + GAP_H + GROUND_H));
  sprPipe.pushSprite(pipeX, 0, 0, 0, PIPE_W, pipeGapY);

  // Vẽ chim
  sprBird.pushSprite(birdX, birdY, TFT_TRANSPARENT);

  // Vẽ nền đất
  sprGround.pushSprite(0, SCREEN_H - GROUND_H);

  // Vẽ điểm số
  tft.fillRect(0, 0, 60, 10, TFT_CYAN);
  tft.setTextColor(TFT_BLACK);
  tft.setCursor(5, 2);
  tft.print("Score: ");
  tft.print(score);

  // Game Over
  if (gameOver) {
    tft.fillRect(30, 30, 100, 20, TFT_WHITE);
    tft.drawRect(30, 30, 100, 20, TFT_BLACK);
    tft.setCursor(50, 37);
    tft.print("GAME OVER");
  }
}
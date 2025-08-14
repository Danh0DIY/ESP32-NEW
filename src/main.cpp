#include <Arduino.h>
#include <TFT_eSPI.h>

#define BTN_JUMP 23

TFT_eSPI tft = TFT_eSPI();

#define SCREEN_W 160
#define SCREEN_H 80
#define GROUND_H 10
#define PIPE_W   20
#define GAP_H    30
#define GRAVITY  0.5
#define JUMP_VEL -3.5

// Bird
float birdY = 40, birdVel = 0;
int birdX = 30;

// Pipe
int pipeX = SCREEN_W, pipeGapY = 30;

// Score
int score = 0;
bool gameOver = false;

// Sprite
TFT_eSprite sprBird = TFT_eSprite(&tft);
TFT_eSprite sprPipe = TFT_eSprite(&tft);
TFT_eSprite sprGround = TFT_eSprite(&tft);

unsigned long lastFrame = 0;
const int FRAME_TIME = 25; // ~80 FPS

// Lưu vị trí cũ
int prevBirdX, prevBirdY;
int prevPipeX, prevPipeGapY;
bool prevGameOver = false;

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

  createBirdSprite();
  createPipeSprite();
  createGroundSprite();

  tft.setTextColor(TFT_BLACK);
  tft.setCursor(20, 35);
  tft.print("Flappy Bird ESP32");
  delay(1000);

  resetGame();
}

void loop() {
  unsigned long now = millis();
  if (now - lastFrame < FRAME_TIME) return;
  lastFrame = now;

  // Input
  static unsigned long lastBtn = 0;
  if (digitalRead(BTN_JUMP) == LOW && millis() - lastBtn > 150) {
    birdVel = JUMP_VEL;
    lastBtn = millis();
  }

  // Update
  if (!gameOver) {
    birdVel += GRAVITY;
    birdY += birdVel;

    pipeX -= 2;
    if (pipeX < -PIPE_W) {
      pipeX = SCREEN_W;
      pipeGapY = random(10, SCREEN_H - GAP_H - GROUND_H - 10);
      score++;
    }

    // Collision
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

  // ==== Clear vùng cũ ====
  // Clear chim
  tft.fillRect(prevBirdX, prevBirdY, 8, 8, TFT_CYAN);
  // Clear ống trên và dưới
  tft.fillRect(prevPipeX, 0, PIPE_W, prevPipeGapY, TFT_CYAN);
  tft.fillRect(prevPipeX, prevPipeGapY + GAP_H, PIPE_W,
               SCREEN_H - (prevPipeGapY + GAP_H + GROUND_H), TFT_CYAN);
  // Clear bảng game over
  if (prevGameOver) {
    tft.fillRect(30, 30, 100, 20, TFT_CYAN);
  }

  // ==== Vẽ mới ====
  // Ống
  sprPipe.pushSprite(pipeX, 0, 0, 0, PIPE_W, pipeGapY);
  sprPipe.pushSprite(pipeX, pipeGapY + GAP_H, 0, pipeGapY + GAP_H,
                     PIPE_W, SCREEN_H - (pipeGapY + GAP_H + GROUND_H));
  // Chim
  sprBird.pushSprite(birdX, birdY, TFT_TRANSPARENT);
  // Nền đất
  sprGround.pushSprite(0, SCREEN_H - GROUND_H);

  // Điểm số
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

  // Lưu vị trí hiện tại
  prevBirdX = birdX;
  prevBirdY = birdY;
  prevPipeX = pipeX;
  prevPipeGapY = pipeGapY;
  prevGameOver = gameOver;
}
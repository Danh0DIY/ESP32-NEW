#include <Arduino.h>
#include <TFT_eSPI.h>
#include <EEPROM.h>

#define BTN_JUMP 23
#define EEPROM_SIZE 4  // lưu 1 biến int

TFT_eSPI tft = TFT_eSPI();

#define SCREEN_W 160
#define SCREEN_H 80
#define GROUND_H 10
#define PIPE_W   20
#define GAP_H    30
#define GRAVITY  0.18   // Rơi chậm hơn
#define JUMP_VEL -2.1

// Bird
float birdY = 20, birdVel = 0;
int birdX = 30;

// Pipe
int pipeX = SCREEN_W, pipeGapY = 30;

// Score
int score = 0;
int highScore = 0;
bool gameOver = false;

// Menu state
enum GameState {MENU, GAME, SCORE_SCREEN};
GameState state = MENU;

// Sprite
TFT_eSprite sprBird = TFT_eSprite(&tft);
TFT_eSprite sprPipe = TFT_eSprite(&tft);
TFT_eSprite sprGround = TFT_eSprite(&tft);

unsigned long lastFrame = 0;
const int FRAME_TIME = 30; // ~80 FPS

// Vị trí cũ để clear
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

void drawMenu() {
  tft.fillScreen(TFT_CYAN);
  tft.setTextColor(TFT_BLACK, TFT_CYAN);
  tft.setTextSize(2);
  tft.setCursor(30, 20);
  tft.print("FLAPPY BIRD");
  tft.setTextSize(1);
  tft.setCursor(45, 45);
  tft.print("Bam nut de choi");
  tft.setCursor(40, 60);
  tft.print("Diem cao: ");
  tft.print(highScore);
}

void drawScoreScreen() {
  tft.fillScreen(TFT_CYAN);
  tft.setTextColor(TFT_BLACK, TFT_CYAN);
  tft.setTextSize(2);
  tft.setCursor(35, 25);
  tft.print("GAME OVER");
  tft.setTextSize(1);
  tft.setCursor(40, 50);
  tft.print("Diem: ");
  tft.print(score);
  tft.setCursor(40, 65);
  tft.print("Diem cao: ");
  tft.print(highScore);
}

void saveHighScore(int s) {
  if (s > highScore) {
    highScore = s;
    EEPROM.writeInt(0, highScore);
    EEPROM.commit();
  }
}

void setup() {
  pinMode(BTN_JUMP, INPUT_PULLUP);
  EEPROM.begin(EEPROM_SIZE);
  highScore = EEPROM.readInt(0);

  tft.init();
  tft.setRotation(1);
  randomSeed(analogRead(0));

  createBirdSprite();
  createPipeSprite();
  createGroundSprite();

  drawMenu();
}

void loop() {
  static unsigned long lastBtn = 0;
  bool btnPress = (digitalRead(BTN_JUMP) == LOW && millis() - lastBtn > 150);
  if (btnPress) lastBtn = millis();

  unsigned long now = millis();
  if (now - lastFrame < FRAME_TIME) return;
  lastFrame = now;

  if (state == MENU) {
    if (btnPress) {
      resetGame();
      state = GAME;
    }
    return;
  }

  if (state == SCORE_SCREEN) {
    if (btnPress) {
      drawMenu();
      state = MENU;
    }
    return;
  }

  // ==== GAME ====
  if (btnPress) birdVel = JUMP_VEL;

  if (!gameOver) {
    birdVel += GRAVITY;
    birdY += birdVel;

    pipeX -= 2;
    if (pipeX < -PIPE_W) {
      pipeX = SCREEN_W;
      pipeGapY = random(10, SCREEN_H - GAP_H - GROUND_H - 10);
      score++;
    }

    if (birdY <= 0 || birdY + 8 >= SCREEN_H - GROUND_H ||
        (birdX + 8 > pipeX && birdX < pipeX + PIPE_W &&
         (birdY < pipeGapY || birdY + 8 > pipeGapY + GAP_H))) {
      gameOver = true;
      saveHighScore(score);
    }
  } else {
    if (btnPress) {
      drawScoreScreen();
      state = SCORE_SCREEN;
    }
  }

  // ==== Clear vùng cũ ====
  tft.fillRect(prevBirdX, prevBirdY, 8, 8, TFT_CYAN);
  tft.fillRect(prevPipeX, 0, PIPE_W, prevPipeGapY, TFT_CYAN);
  tft.fillRect(prevPipeX, prevPipeGapY + GAP_H, PIPE_W,
               SCREEN_H - (prevPipeGapY + GAP_H + GROUND_H), TFT_CYAN);
  if (prevGameOver) {
    tft.fillRect(30, 30, 100, 20, TFT_CYAN);
  }

  // ==== Vẽ mới ====
  sprPipe.pushSprite(pipeX, 0, 0, 0, PIPE_W, pipeGapY);
  sprPipe.pushSprite(pipeX, pipeGapY + GAP_H, 0, pipeGapY + GAP_H,
                     PIPE_W, SCREEN_H - (pipeGapY + GAP_H + GROUND_H));

  sprBird.pushSprite(birdX, birdY, TFT_TRANSPARENT);
  sprGround.pushSprite(0, SCREEN_H - GROUND_H);

  tft.fillRect(0, 0, 60, 10, TFT_CYAN);
  tft.setTextColor(TFT_BLACK);
  tft.setCursor(5, 2);
  tft.print("Score: ");
  tft.print(score);

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
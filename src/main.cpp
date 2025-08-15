// Flappy Bird ESP32 + ST7735S (160x80)
// - Nút nhảy: GPIO 23 (đã khai báo)
// - Menu, High Score lưu bằng Preferences
// - Sprite chim 2 frame (vỗ cánh)
// - Partial redraw with proper clear -> no ghosting

#include <Arduino.h>
#include <TFT_eSPI.h>    // cấu hình chân trong User_Setup.h
#include <Preferences.h>

#define BUTTON_PIN 23   // nút nhảy ở chân 23 (pull-up)
TFT_eSPI tft = TFT_eSPI();
Preferences prefs;

// --- Screen / Game config ---
const int SCREEN_W = 160;
const int SCREEN_H = 80;
const int GROUND_H = 10;
const int PIPE_W = 20;
const int GAP_H = 30;

// Thay đổi để tinh chỉnh độ khó:
float GRAVITY = 0.18f;     // giảm => rơi chậm hơn
float JUMP_VEL = -2.1f;   // âm => nhảy lên
int PIPE_SPEED = 2;       // tốc độ ống di chuyển

// Bird
float birdY = 40.0f;
float birdVel = 0.0f;
const int birdX = 30;
const int BIRD_W = 8;
const int BIRD_H = 8;

// Pipe
int pipeX = SCREEN_W;
int pipeGapY = 30;

// Score
int score = 0;
unsigned int highScore = 0;

// Game state
enum GameState { STATE_MENU, STATE_PLAY, STATE_SCORE };
GameState state = STATE_MENU;
bool gameOver = false;

// Sprites
TFT_eSprite sprBirdA = TFT_eSprite(&tft);
TFT_eSprite sprBirdB = TFT_eSprite(&tft);
TFT_eSprite sprPipe = TFT_eSprite(&tft);
TFT_eSprite sprGround = TFT_eSprite(&tft);

// Previous positions (for clearing)
int prevBirdX = -100, prevBirdY = -100;
int prevPipeX = -100, prevPipeGapY = -100;
bool prevGameOver = false;

// Timing
unsigned long lastFrame = 0;
const int FRAME_TIME = 30; // ~80 FPS
int animFrameCounter = 0;

// Debounce
unsigned long lastBtnTime = 0;
const unsigned long DEBOUNCE_MS = 120;

// ---------- Helper: safe fillRect that clips to screen ----------
void fillRectClip(int x, int y, int w, int h, uint16_t color) {
  if (w <= 0 || h <= 0) return;
  if (x + w <= 0 || y + h <= 0 || x >= SCREEN_W || y >= SCREEN_H) return;
  int nx = max(0, x);
  int ny = max(0, y);
  int nw = min(SCREEN_W - nx, x + w - nx);
  int nh = min(SCREEN_H - ny, y + h - ny);
  tft.fillRect(nx, ny, nw, nh, color);
}

// ---------- Create sprites ----------
void createBirdSprites() {
  // Frame A (wings up)
  sprBirdA.createSprite(BIRD_W, BIRD_H);
  sprBirdA.fillSprite(TFT_TRANSPARENT);
  sprBirdA.fillCircle(4, 4, 4, TFT_YELLOW); // thân
  sprBirdA.fillCircle(2, 3, 1, TFT_BLACK);  // mắt
  sprBirdA.fillRect(6, 3, 2, 2, TFT_ORANGE); // mỏ
  sprBirdA.fillRect(1, 0, 6, 2, TFT_YELLOW); // cánh up (simple)

  // Frame B (wings down)
  sprBirdB.createSprite(BIRD_W, BIRD_H);
  sprBirdB.fillSprite(TFT_TRANSPARENT);
  sprBirdB.fillCircle(4, 4, 4, TFT_YELLOW);
  sprBirdB.fillCircle(2, 3, 1, TFT_BLACK);
  sprBirdB.fillRect(6, 3, 2, 2, TFT_ORANGE);
  sprBirdB.fillRect(1, 5, 6, 2, TFT_YELLOW); // cánh down
}

void createPipeSprite() {
  sprPipe.createSprite(PIPE_W, SCREEN_H);
  sprPipe.fillSprite(TFT_GREEN);
  sprPipe.drawRect(0, 0, PIPE_W, SCREEN_H, TFT_DARKGREEN);
  // small shading stripes
  for (int y = 0; y < SCREEN_H; y += 6) {
    sprPipe.drawLine(0, y, PIPE_W-1, y, TFT_DARKGREY);
  }
}

void createGroundSprite() {
  sprGround.createSprite(SCREEN_W, GROUND_H);
  sprGround.fillSprite(TFT_BROWN);
  for (int i=0;i<SCREEN_W;i+=4) sprGround.fillRect(i, GROUND_H-2, 2, 2, TFT_DARKGREY);
}

// ---------- Reset / Save ----------
void resetGame() {
  birdY = SCREEN_H / 2;
  birdVel = 0;
  pipeX = SCREEN_W;
  pipeGapY = random(10, SCREEN_H - GAP_H - GROUND_H - 10);
  score = 0;
  gameOver = false;
  // set prev positions to offscreen to avoid clearing valid areas on first frame
  prevBirdX = prevBirdY = prevPipeX = prevPipeGapY = -100;
  prevGameOver = false;
}

void saveHighScore() {
  if ((unsigned)score > highScore) {
    highScore = score;
    prefs.putUInt("high", highScore);
  }
}

// ---------- Draw menu / screens ----------
void drawMenu() {
  tft.fillScreen(TFT_CYAN);
  tft.setTextSize(1.5);
  tft.setTextColor(TFT_BLACK, TFT_CYAN);
  tft.setCursor(18, 12);
  tft.print("FLAPPY BIRD");
  tft.setTextSize(1);
  tft.setCursor(40, 40);
  tft.print("Bam nut de choi");
  tft.setCursor(30, 55);
  tft.print("Diem cao: ");
  tft.print(highScore);
}

void drawScoreScreen() {
  tft.fillScreen(TFT_CYAN);
  tft.setTextSize(2);
  tft.setTextColor(TFT_BLACK, TFT_CYAN);
  tft.setCursor(25, 18);
  tft.print("GAME OVER");
  tft.setTextSize(1);
  tft.setCursor(30, 45);
  tft.print("Diem: ");
  tft.print(score);
  tft.setCursor(30, 58);
  tft.print("Diem cao: ");
  tft.print(highScore);
}

// ---------- Setup ----------
void setup() {
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  Serial.begin(115200);

  prefs.begin("flappy", false);
  highScore = prefs.getUInt("high", 0);

  tft.init();
  tft.setRotation(1);
  randomSeed(analogRead(0));

  createBirdSprites();
  createPipeSprite();
  createGroundSprite();

  drawMenu();
}

// ---------- Main Loop ----------
void loop() {
  unsigned long now = millis();
  if (now - lastFrame < FRAME_TIME) return; // frame cap
  lastFrame = now;
  animFrameCounter++;

  // Read button with debounce
  bool btn = false;
  if (digitalRead(BUTTON_PIN) == LOW) {
    if (millis() - lastBtnTime > DEBOUNCE_MS) {
      btn = true;
      lastBtnTime = millis();
    }
  }

  // State handling
  if (state == STATE_MENU) {
    if (btn) {
      resetGame();
      state = STATE_PLAY;
      // draw initial background for game
      tft.fillScreen(TFT_CYAN);
      sprGround.pushSprite(0, SCREEN_H - GROUND_H);
    }
    return;
  }

  if (state == STATE_SCORE) {
    if (btn) {
      drawMenu();
      state = STATE_MENU;
    }
    return;
  }

  // ----- GAME PLAY -----
  if (btn && !gameOver) {
    birdVel = JUMP_VEL;
  }

  if (!gameOver) {
    birdVel += GRAVITY;
    birdY += birdVel;

    pipeX -= PIPE_SPEED;
    if (pipeX < -PIPE_W) {
      pipeX = SCREEN_W;
      pipeGapY = random(10, SCREEN_H - GAP_H - GROUND_H - 10);
      score++;
    }

    // collision check
    if (birdY <= 0 || birdY + BIRD_H >= SCREEN_H - GROUND_H ||
        (birdX + BIRD_W > pipeX && birdX < pipeX + PIPE_W &&
         (birdY < pipeGapY || birdY + BIRD_H > pipeGapY + GAP_H))) {
      gameOver = true;
      saveHighScore();
    }
  } else {
    // when dead, press once to go score screen
    if (btn) {
      drawScoreScreen();
      state = STATE_SCORE;
    }
  }

  // ---------- Clear old regions (critical to prevent ghosting) ----------
  // Clear previous bird
  fillRectClip(prevBirdX, prevBirdY, BIRD_W, BIRD_H, TFT_CYAN);
  // Clear previous pipe top
  if (prevPipeX > -100) {
    fillRectClip(prevPipeX, 0, PIPE_W, prevPipeGapY, TFT_CYAN);
    // Clear previous pipe bottom
    int bottomH = SCREEN_H - (prevPipeGapY + GAP_H + GROUND_H);
    if (bottomH > 0)
      fillRectClip(prevPipeX, prevPipeGapY + GAP_H, PIPE_W, bottomH, TFT_CYAN);
  }
  // Clear previous game over box if existed
  if (prevGameOver) {
    fillRectClip(30, 30, 100, 20, TFT_CYAN);
  }
  // Clear score area (top-left)
  fillRectClip(0, 0, 60, 10, TFT_CYAN);

  // ---------- Draw new objects ----------
  // Draw pipe top
  sprPipe.pushSprite(pipeX, 0, 0, 0, PIPE_W, pipeGapY);
  // Draw pipe bottom
  int bottomY = pipeGapY + GAP_H;
  int bottomH = SCREEN_H - (pipeGapY + GAP_H + GROUND_H);
  if (bottomH > 0) sprPipe.pushSprite(pipeX, bottomY, 0, bottomY, PIPE_W, bottomH);

  // Bird animation: alternate frames every few ticks
  if ((animFrameCounter / 4) % 2 == 0) {
    sprBirdA.pushSprite(birdX, (int)birdY, TFT_TRANSPARENT);
  } else {
    sprBirdB.pushSprite(birdX, (int)birdY, TFT_TRANSPARENT);
  }

  // Ground (always at bottom)
  sprGround.pushSprite(0, SCREEN_H - GROUND_H);

  // Score text
  tft.setTextSize(1);
  tft.setTextColor(TFT_BLACK, TFT_CYAN);
  tft.setCursor(5, 2);
  tft.print("Score: ");
  tft.print(score);

  // Game over box (draw if gameOver)
  if (gameOver) {
    tft.fillRect(30, 30, 100, 20, TFT_WHITE);
    tft.drawRect(30, 30, 100, 20, TFT_BLACK);
    tft.setCursor(42, 37);
    tft.setTextColor(TFT_BLACK, TFT_WHITE);
    tft.print("GAME OVER");
  }

  // ---------- Save current positions for next frame clearing ----------
  prevBirdX = birdX;
  prevBirdY = (int)birdY;
  prevPipeX = pipeX;
  prevPipeGapY = pipeGapY;
  prevGameOver = gameOver;
}
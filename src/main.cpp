/* Snake (ESP32-C3 + ST7735S 160x80 via TFT_eSPI)
   - Lưới 20x10, mỗi ô 8x8 pixel
   - 2 nút: rẽ trái / rẽ phải (INPUT_PULLUP, nhấn = LOW)
   - Va vào thân = GAME OVER, nhấn A hoặc B để chơi lại
   - Xuyên tường: đi qua biên sang đối diện
   - High score lưu trong flash (Preferences)
   - Tạm dừng: giữ đồng thời A+B ~0.6s
*/

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Preferences.h>

TFT_eSPI tft;
Preferences prefs;

// ======= Cấu hình nút =======
#define BUTTON_A 20    // Rẽ trái
#define BUTTON_B 21   // Rẽ phải

// ======= Lưới =======
const int CELL = 8;       // kích thước ô
const int COLS = 20;      // 160 / 8
const int ROWS = 10;      //  80 / 8
const int W = COLS * CELL;
const int H = ROWS * CELL;

// ======= Màu =======
#define C_BG    TFT_BLACK
#define C_GRID  0x39E7  // xám nhạt
#define C_SNAKE TFT_GREEN
#define C_HEAD  TFT_YELLOW
#define C_FOOD  TFT_RED
#define C_TXT   TFT_WHITE

// ======= Trạng thái game =======
enum Dir { UP=0, RIGHT=1, DOWN=2, LEFT=3 };

struct Cell { int x; int y; };
Cell snake[COLS*ROWS];  // tối đa chiếm toàn bộ
int snakeLen = 3;
Dir dirNow = RIGHT;
Dir dirNext = RIGHT;

Cell food = { -1, -1 };

bool paused = false;
bool gameOver = false;
uint32_t lastStep = 0;
uint16_t stepDelay = 220; // ms, sẽ giảm dần khi tăng điểm
uint16_t score = 0;
uint16_t highScore = 0;

// ======= Input helper =======
bool lastA = true, lastB = true;
uint32_t bothDownStart = 0;
const uint16_t bothHoldMs = 600;

bool btnA() { return digitalRead(BUTTON_A) == LOW; }
bool btnB() { return digitalRead(BUTTON_B) == LOW; }

// ======= Utils =======
void drawCell(int cx, int cy, uint16_t color) {
  int px = cx * CELL;
  int py = cy * CELL;
  tft.fillRect(px+1, py+1, CELL-2, CELL-2, color); // chừa viền mảnh
}

bool snakeHits(int x, int y, bool ignoreTail=false) {
  int end = snakeLen - (ignoreTail ? 1 : 0);
  for (int i=0; i<end; ++i) {
    if (snake[i].x == x && snake[i].y == y) return true;
  }
  return false;
}

void spawnFood() {
  if (snakeLen >= COLS*ROWS) { food = {-1,-1}; return; }
  int tries = 0;
  do {
    food.x = random(0, COLS);
    food.y = random(0, ROWS);
    tries++;
  } while (snakeHits(food.x, food.y) && tries < 2000);
  drawCell(food.x, food.y, C_FOOD);
}

void drawHUD() {
  tft.setTextDatum(TL_DATUM);
  tft.setTextColor(C_TXT, C_BG);
  tft.setTextSize(1);
  // nền cho HUD
  tft.fillRect(0, 0, W, 8, C_BG);
  tft.setCursor(2, 0);
  tft.printf("Score:%u  Best:%u%s", score, highScore, paused ? "  [PAUSE]" : "");
}

void drawGrid() {
  tft.fillScreen(C_BG);
  // kẻ lưới mảnh cho nhìn dễ
  for (int x=0; x<=W; x+=CELL) tft.drawFastVLine(x, 0, H, C_GRID);
  for (int y=0; y<=H; y+=CELL) tft.drawFastHLine(0, y, W, C_GRID);
}

void resetGame() {
  // Snake bắt đầu giữa màn
  snakeLen = 3;
  int cx = COLS/2 - 2;
  int cy = ROWS/2;
  snake[0] = {cx+2, cy};
  snake[1] = {cx+1, cy};
  snake[2] = {cx,   cy};
  dirNow = RIGHT;
  dirNext = RIGHT;
  score = 0;
  stepDelay = 220;
  gameOver = false;
  paused = false;

  drawGrid();
  // vẽ snake
  for (int i=0; i<snakeLen; ++i) drawCell(snake[i].x, snake[i].y, i==0?C_HEAD:C_SNAKE);
  spawnFood();
  drawHUD();
}

void gameOverScreen() {
  tft.fillScreen(C_BG);
  tft.setTextColor(C_TXT, C_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.drawString("GAME OVER", W/2, H/2 - 10);
  tft.setTextSize(1);
  char buf[48];
  sprintf(buf, "Score: %u   Best: %u", score, highScore);
  tft.drawString(buf, W/2, H/2 + 8);
  tft.drawString("Press A/B to restart", W/2, H/2 + 24);
}

void handleInput() {
  bool aNow = btnA();
  bool bNow = btnB();

  // Nhấn đồng thời A+B để toggle PAUSE (giữ ~0.6s)
  if (aNow && bNow) {
    if (bothDownStart == 0) bothDownStart = millis();
    else if (!gameOver && millis() - bothDownStart >= bothHoldMs) {
      paused = !paused;
      bothDownStart = 0;
      drawHUD();
    }
  } else {
    bothDownStart = 0;
  }

  // Nhấn đơn: đổi hướng tương đối (A: quay trái, B: quay phải)
  if (!gameOver && !paused) {
    if (aNow && !lastA) {
      dirNext = (Dir)((dirNow + 3) & 3); // trái
    }
    if (bNow && !lastB) {
      dirNext = (Dir)((dirNow + 1) & 3); // phải
    }
  }

  // Restart khi game over
  if (gameOver && (aNow && !lastA || bNow && !lastB)) {
    resetGame();
  }

  lastA = aNow;
  lastB = bNow;
}

void stepSnake() {
  if (paused || gameOver) return;

  // cập nhật hướng, chặn quay ngược 180°
  if (!((dirNow == UP && dirNext == DOWN) ||
        (dirNow == DOWN && dirNext == UP) ||
        (dirNow == LEFT && dirNext == RIGHT) ||
        (dirNow == RIGHT && dirNext == LEFT))) {
    dirNow = dirNext;
  }

  // Tính đầu mới
  int nx = snake[0].x;
  int ny = snake[0].y;
  if (dirNow == UP)    ny--;
  if (dirNow == DOWN)  ny++;
  if (dirNow == LEFT)  nx--;
  if (dirNow == RIGHT) nx++;

  // ======= Xuyên tường =======
  if (nx < 0) nx = COLS-1;
  if (nx >= COLS) nx = 0;
  if (ny < 0) ny = ROWS-1;
  if (ny >= ROWS) ny = 0;

  // Ăn mồi?
  bool grow = (food.x == nx && food.y == ny);

  // Kiểm tra cắn thân
  if (snakeHits(nx, ny, !grow)) {
    gameOver = true;
    if (score > highScore) {
      highScore = score;
      prefs.putUInt("highscore", highScore); // lưu vào flash
    }
    gameOverScreen();
    return;
  }

  // Vẽ đuôi cũ thành nền (nếu không grow)
  Cell tail = snake[snakeLen-1];
  if (!grow) {
    for (int i=snakeLen-1; i>0; --i) snake[i] = snake[i-1];
    drawCell(tail.x, tail.y, C_BG);
  } else {
    for (int i=snakeLen; i>0; --i) snake[i] = snake[i-1];
    snakeLen++;
  }

  // đặt đầu mới
  snake[0] = {nx, ny};

  // Vẽ: đầu mới, và biến ô đầu cũ thành thân xanh
  if (snakeLen > 1) drawCell(snake[1].x, snake[1].y, C_SNAKE);
  drawCell(nx, ny, C_HEAD);

  // Nếu vừa ăn
  if (grow) {
    score++;
    if (stepDelay > 90 && (score % 4 == 0)) stepDelay -= 10;
    spawnFood();
    drawHUD();
  }
}

void setup() {
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);

  randomSeed((uint32_t)analogRead(A0) ^ micros());

  tft.init();
  tft.setRotation(1); // ngang: 160x80
  tft.fillScreen(C_BG);
  tft.setSwapBytes(true);

  prefs.begin("snake", false);
  highScore = prefs.getUInt("highscore", 0);

  lastA = !btnA();
  lastB = !btnB();

  resetGame();
  lastStep = millis();
}

void loop() {
  handleInput();

  uint32_t now = millis();
  if (now - lastStep >= stepDelay) {
    lastStep = now;
    stepSnake();
  }
}
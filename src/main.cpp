#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();

// Nút bấm
#define BTN_LEFT  20
#define BTN_RIGHT 21

// Paddle
int paddleX = 60;
int paddleY = 75;
int paddleW = 30;
int paddleH = 5;
int prevPaddleX = paddleX;

// Bóng
int ballX = 80, ballY = 40;
int ballSize = 4;
int ballDX = 1, ballDY = 1;
int prevBallX = ballX, prevBallY = ballY;

// Brick
const int brickRowsMax = 6;
const int brickCols = 6;
const int brickW = 24;
const int brickH = 8;
bool bricks[brickRowsMax][brickCols];

// Game info
int score = 0;
int level = 1;
int brickRows = 3;   // số hàng ban đầu
int lives = 3;
bool isGameOver = false;

unsigned long lastBtnPress = 0; // đơn giản debounce cho reset

// --- Hàm khai báo ---
void drawPaddle(int x, int y, uint16_t color) {
  tft.fillRect(x, y, paddleW, paddleH, color);
}

void drawBall(int x, int y, uint16_t color) {
  tft.fillRect(x, y, ballSize, ballSize, color);
}

void drawBrick(int col, int row, uint16_t color) {
  int x = col * brickW;
  int y = row * brickH;
  tft.fillRect(x + 1, y + 1, brickW - 2, brickH - 2, color);
}

void showInfo() {
  // xóa thanh trên (chỉ vùng HUD)
  tft.fillRect(0, 0, 160, 8, TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(2, 0);
  tft.print("Sc:");
  tft.print(score);
  tft.setCursor(70, 0);
  tft.print("Lv:");
  tft.print(level);
  tft.setCursor(110, 0);
  tft.print("L:");
  tft.print(lives);
}

bool allBricksCleared() {
  for (int r = 0; r < brickRows; r++) {
    for (int c = 0; c < brickCols; c++) {
      if (bricks[r][c]) return false;
    }
  }
  return true;
}

void resetBallPaddlePreserveBricks() {
  // Giữ lại bricks (dùng khi mất mạng nhưng chưa hết mạng)
  // Xóa vết cũ trên màn hiển thị
  tft.fillRect(prevBallX, prevBallY, ballSize, ballSize, TFT_BLACK);
  tft.fillRect(prevPaddleX, paddleY, paddleW, paddleH, TFT_BLACK);

  paddleX = 60;
  prevPaddleX = paddleX;
  drawPaddle(paddleX, paddleY, TFT_WHITE);

  ballX = 80;
  ballY = 40;
  // Ball tốc độ tùy level
  int sp = 1 + (level - 1) / 2;
  ballDX = (random(0, 2) == 0) ? sp : -sp;
  ballDY = -sp;
  prevBallX = ballX; prevBallY = ballY;
  drawBall(ballX, ballY, TFT_YELLOW);

  showInfo();
}

void resetGameFull() {
  // Reset toàn bộ (khi start game hoặc after Game Over)
  tft.fillScreen(TFT_BLACK);

  paddleX = 60;
  paddleY = 75;
  prevPaddleX = paddleX;
  drawPaddle(paddleX, paddleY, TFT_WHITE);

  ballX = 80;
  ballY = 40;
  ballDX = 1;
  ballDY = 1;
  prevBallX = ballX; prevBallY = ballY;
  drawBall(ballX, ballY, TFT_YELLOW);

  // Reset bricks theo số hàng hiện tại
  for (int r = 0; r < brickRowsMax; r++) {
    for (int c = 0; c < brickCols; c++) {
      if (r < brickRows) {
        bricks[r][c] = true;
        drawBrick(c, r, (r % 2 == 0) ? TFT_GREEN : TFT_CYAN);
      } else {
        bricks[r][c] = false;
      }
    }
  }

  showInfo();
}

void checkBrickCollision() {
  // Dò col,row theo tâm bóng (đơn giản)
  int cx = ballX + ballSize/2;
  int cy = ballY + ballSize/2;
  int row = cy / brickH;
  int col = cx / brickW;

  if (row >= 0 && row < brickRows && col >= 0 && col < brickCols) {
    if (bricks[row][col]) {
      bricks[row][col] = false;
      drawBrick(col, row, TFT_BLACK); // Xóa brick
      ballDY = -ballDY;
      score += 10;
      showInfo();

      if (allBricksCleared()) {
        // qua màn
        level++;
        brickRows = min(brickRows + 1, brickRowsMax);
        // tăng tốc bóng 1 bậc
        int sp = 1 + (level - 1) / 2;
        if (ballDX > 0) ballDX = sp; else ballDX = -sp;
        ballDY = (ballDY > 0) ? sp : -sp;

        delay(300);
        resetGameFull();
      }
    }
  }
}

void setup() {
  pinMode(BTN_LEFT, INPUT_PULLUP);
  pinMode(BTN_RIGHT, INPUT_PULLUP);

  tft.init();
  tft.setRotation(3);   // xoay ngang
  tft.fillScreen(TFT_BLACK);

  randomSeed(analogRead(0));

  // Khởi tạo game
  score = 0;
  level = 1;
  brickRows = 3;
  lives = 3;
  isGameOver = false;

  resetGameFull();
}

void loop() {
  // Nếu đang ở trạng thái Game Over -> chờ nhấn nút để restart toàn bộ
  if (isGameOver) {
    // chờ nhấn BTN_LEFT hoặc BTN_RIGHT để restart (có debounce nhỏ)
    if (digitalRead(BTN_LEFT) == LOW || digitalRead(BTN_RIGHT) == LOW) {
      if (millis() - lastBtnPress > 200) {
        // restart toàn bộ game
        score = 0;
        level = 1;
        brickRows = 3;
        lives = 3;
        isGameOver = false;
        resetGameFull();
      }
      lastBtnPress = millis();
    }
    return;
  }

  // ---- Paddle input ----
  bool paddleMoved = false;
  if (digitalRead(BTN_LEFT) == LOW && paddleX > 0) {
    prevPaddleX = paddleX;
    paddleX -= 2;
    paddleMoved = true;
  }
  if (digitalRead(BTN_RIGHT) == LOW && paddleX < 160 - paddleW) {
    prevPaddleX = paddleX;
    paddleX += 2;
    paddleMoved = true;
  }

  // Xóa và vẽ paddle chỉ khi di chuyển
  if (paddleMoved) {
    tft.fillRect(prevPaddleX, paddleY, paddleW, paddleH, TFT_BLACK);
    drawPaddle(paddleX, paddleY, TFT_WHITE);
  }

  // ---- Ball update ----
  prevBallX = ballX; prevBallY = ballY;
  ballX += ballDX;
  ballY += ballDY;

  // Va chạm tường trái/phải
  if (ballX <= 0) {
    ballX = 0;
    ballDX = -ballDX;
  } else if (ballX >= 160 - ballSize) {
    ballX = 160 - ballSize;
    ballDX = -ballDX;
  }
  // Va chạm trần
  if (ballY <= 8) { // giữ 8px để không chồng lên HUD
    ballY = 8;
    ballDY = -ballDY;
  }

  // Va chạm paddle
  if (ballY + ballSize >= paddleY && ballX + ballSize >= paddleX && ballX <= paddleX + paddleW) {
    ballDY = -abs(ballDY); // bật lên
    ballY = paddleY - ballSize;
    // khi chạm paddle có thể thay đổi hướng ngang dựa vào phần gặp paddle
    int hitPos = (ballX + ballSize/2) - (paddleX + paddleW/2); // -.....+
    if (hitPos < 0) ballDX = max(-3, ballDX -  (abs(hitPos)/5));
    else ballDX = min(3, ballDX + (abs(hitPos)/5));
  }

  // Va chạm brick
  checkBrickCollision();

  // Nếu rơi xuống dưới -> mất mạng
  if (ballY >= 80 - ballSize) {
    lives--;
    showInfo();
    if (lives > 0) {
      // reset ball & paddle, giữ bricks + score + level
      delay(200);
      resetBallPaddlePreserveBricks();
    } else {
      // Game Over
      isGameOver = true;
      // Hiện hộp Game Over
      tft.fillRect(30, 28, 100, 24, TFT_WHITE);
      tft.drawRect(30, 28, 100, 24, TFT_BLACK);
      tft.setTextColor(TFT_BLACK, TFT_WHITE);
      tft.setCursor(45, 34);
      tft.print("GAME OVER");
      tft.setCursor(38, 44);
      tft.print("Press Btn to restart");
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      return; // dừng update tiếp
    }
  }

  // Xoá bóng cũ, vẽ bóng mới
  tft.fillRect(prevBallX, prevBallY, ballSize, ballSize, TFT_BLACK);
  drawBall(ballX, ballY, TFT_YELLOW);

  delay(10); // điều hòa tốc độ loop
}
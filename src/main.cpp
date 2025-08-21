#include <TFT_eSPI.h>
#include <SPI.h>
#include <math.h>

TFT_eSPI tft = TFT_eSPI();

// ------------------ Nút bấm ------------------
#define BTN_LEFT 20
#define BTN_RIGHT 21

// ------------------ Paddle -------------------
int paddleX = 60;
const int paddleY = 75;
int paddleW = 30;
const int paddleH = 5;
const int paddleSpeed = 3;

// Paddle power-up timer
unsigned long paddlePowerEnd = 0;

// ------------------ Ball ---------------------
const int MAX_BALLS = 3;
struct Ball {
  float x, y;
  float vx, vy;
  bool active;
};

Ball balls[MAX_BALLS];

// ------------------ Brick --------------------
const int brickRowsMax = 6;
const int brickCols = 6;
const int brickW = 24;
const int brickH = 8;
bool bricks[brickRowsMax][brickCols];
int brickRows = 3;

// ------------------ PowerUp ------------------
struct PowerUp {
  float x, y;
  int type; // 0=paddle+long,1=multi-ball,2=slow,3=life
  bool active;
};

#define MAX_POWERUPS 3
PowerUp powerups[MAX_POWERUPS];

// ------------------ Game info ----------------
int score = 0;
int level = 1;
int lives = 3;
bool isGameOver = false;
unsigned long lastBtnPress = 0;
float ballSpeed = 3.0;
float slowBallFactor = 0.6; // giảm tốc khi Power-up Slow

// ------------------ Previous positions -------------
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

// ------------------ Loop ------------------------
void loop() {
  if (isGameOver) {
    if ((digitalRead(BTN_LEFT) == LOW || digitalRead(BTN_RIGHT) == LOW) && millis() - lastBtnPress > 200) {
      score = 0; level = 1; brickRows = 3; lives = 3; isGameOver = false;
      resetGameFull();
      lastBtnPress = millis();
    }
    return;
  }

  handlePaddle();
  updateBalls();
  updatePowerUps();
  drawHUD();

  delay(10);
}

// ------------------ Paddle ----------------------
void handlePaddle() {
  prevPaddleX = paddleX;
  if (digitalRead(BTN_LEFT) == LOW && paddleX > 0) paddleX -= paddleSpeed;
  if (digitalRead(BTN_RIGHT) == LOW && paddleX + paddleW < 160) paddleX += paddleSpeed;

  // Kiểm tra paddle Power-up hết hạn
  if (paddlePowerEnd > 0 && millis() > paddlePowerEnd) {
    paddleW = 30;
    paddlePowerEnd = 0;
  }

  if (prevPaddleX != paddleX) {
    tft.fillRect(prevPaddleX, paddleY, paddleW, paddleH, TFT_BLACK);
    drawPaddle();
  }
}

void drawPaddle() { tft.fillRect(paddleX, paddleY, paddleW, paddleH, TFT_WHITE); }

// ------------------ Ball ------------------------
void initBall(int idx, float x, float y, float vx, float vy) {
  balls[idx].x = x;
  balls[idx].y = y;
  balls[idx].vx = vx;
  balls[idx].vy = vy;
  balls[idx].active = true;
  prevBallX[idx] = x; prevBallY[idx] = y;
}

void updateBalls() {
  for (int i=0;i<MAX_BALLS;i++){
    if (!balls[i].active) continue;

    // Xóa bóng cũ
    tft.fillRect(prevBallX[i], prevBallY[i], 4,4,TFT_BLACK);

    balls[i].x += balls[i].vx;
    balls[i].y += balls[i].vy;

    // Va chạm tường
    if (balls[i].x <= 0){ balls[i].x=0; balls[i].vx=fabs(balls[i].vx);}
    if (balls[i].x+4 >=160){ balls[i].x=156; balls[i].vx=-fabs(balls[i].vx);}
    if (balls[i].y <=8){ balls[i].y=8; balls[i].vy=-balls[i].vy;}

    // Va chạm paddle
    if (balls[i].y+4 >= paddleY && balls[i].x+4 >= paddleX && balls[i].x <= paddleX + paddleW) {
      balls[i].vy = -fabs(balls[i].vy);

      float hitPos = (balls[i].x+2) - (paddleX+paddleW/2);
      balls[i].vx = hitPos * 0.15;

      // chuẩn hóa tốc độ
      float len = sqrt(balls[i].vx*balls[i].vx + balls[i].vy*balls[i].vy);
      balls[i].vx = (balls[i].vx / len) * ballSpeed;
      balls[i].vy = (balls[i].vy / len) * ballSpeed;

      balls[i].y = paddleY -4;
    }

    // Va chạm brick
    int row = (int)(balls[i].y / brickH);
    int col = (int)(balls[i].x / brickW);
    if (row >=0 && row < brickRows && col >=0 && col < brickCols && bricks[row][col]){
      bricks[row][col] = false;
      tft.fillRect(col*brickW+1,row*brickH+1,brickW-2,brickH-2,TFT_BLACK);
      balls[i].vy = -balls[i].vy;
      score +=10;

      // Tạo Power-up 20% cơ hội
      if (random(100)<20) spawnPowerUp(col*brickW+brickW/2,row*brickH+brickH/2);
      if (allBricksCleared()) { level++; brickRows=min(brickRows+1,brickRowsMax); delay(300); resetGameFull(); return; }
    }

    // Rơi xuống
    if (balls[i].y+4 >=80) {
      balls[i].active=false;
      checkGameOver();
      continue;
    }

    drawBall(i);
    prevBallX[i] = balls[i].x;
    prevBallY[i] = balls[i].y;
  }
}

void drawBall(int idx){ tft.fillRect(balls[idx].x,balls[idx].y,4,4,TFT_RED); }

bool allBricksCleared(){
  for (int r=0;r<brickRows;r++) for(int c=0;c<brickCols;c++) if (bricks[r][c]) return false;
  return true;
}

void checkGameOver(){
  bool anyActive=false;
  for (int i=0;i<MAX_BALLS;i++) if (balls[i].active) anyActive=true;
  if (!anyActive){
    lives--;
    if (lives>0) resetBallPaddlePreserveBricks();
    else showGameOver();
  }
}

// ------------------ Power-up --------------------
void spawnPowerUp(float x,float y){
  for(int i=0;i<MAX_POWERUPS;i++){
    if (!powerups[i].active){
      powerups[i].x=x; powerups[i].y=y; powerups[i].type=random(4); powerups[i].active=true;
      prevPowerX[i]=x; prevPowerY[i]=y;
      break;
    }
  }
}

void updatePowerUps(){
  for(int i=0;i<MAX_POWERUPS;i++){
    if (!powerups[i].active) continue;
    tft.fillRect(prevPowerX[i],prevPowerY[i],6,6,TFT_BLACK);
    powerups[i].y +=1;

    // Hứng paddle
    if (powerups[i].y+6>=paddleY && powerups[i].x+3 >= paddleX && powerups[i].x-3 <= paddleX+paddleW){
      activatePowerUp(powerups[i].type);
      powerups[i].active=false;
      continue;
    }

    // Rơi xuống đáy
    if (powerups[i].y>80) powerups[i].active=false;

    // Vẽ Power-up
    switch(powerups[i].type){
      case 0: tft.fillRect(powerups[i].x,powerups[i].y,6,6,TFT_BLUE); break; // paddle dài
      case 1: tft.fillRect(powerups[i].x,powerups[i].y,6,6,TFT_YELLOW); break; // multi-ball
      case 2: tft.fillRect(powerups[i].x,powerups[i].y,6,6,TFT_GREEN); break; // slow
      case 3: tft.fillRect(powerups[i].x,powerups[i].y,6,6,TFT_RED); break; // life
    }

    prevPowerX[i]=powerups[i].x; prevPowerY[i]=powerups[i].y;
  }
}

void activatePowerUp(int type){
  switch(type){
    case 0: // paddle dài
      paddleW = 50;
      paddlePowerEnd = millis()+10000;
      break;
    case 1: // multi-ball
      for(int i=0;i<MAX_BALLS;i++){
        if (!balls[i].active){
          initBall(i, balls[0].x, balls[0].y, ballSpeed*cos(random(0,360)*3.14/180.0), -ballSpeed*sin(random(0,360)*3.14/180.0));
          break;
        }
      }
      break;
    case 2: // slow
      ballSpeed=1.5;
      // reset timer để sau 8s trở về bình thường
      paddlePowerEnd=millis()+8000;
      break;
    case 3: // life
      lives++;
      break;
  }
}

// ------------------ Reset / Game Over -------------
void resetGameFull(){
  tft.fillScreen(TFT_BLACK);

  // Paddle
  paddleX=60; paddleW=30; drawPaddle();

  // Ball
  for(int i=0;i<MAX_BALLS;i++) balls[i].active=false;
  initBall(0,80,40,2.0,-2.0);

  // Bricks
  for(int r=0;r<brickRowsMax;r++)
    for(int c=0;c<brickCols;c++){
      if(r<brickRows){ bricks[r][c]=true; tft.fillRect(c*brickW+1,r*brickH+1,brickW-2,brickH-2,(r%2==0?TFT_GREEN:TFT_CYAN));}
      else bricks[r][c]=false;
    }

  // Power-ups
  for(int i=0;i<MAX_POWERUPS;i++) powerups[i].active=false;

  drawHUD();
}

void resetBallPaddlePreserveBricks(){
  paddleX=60; paddleW=30; drawPaddle();
  for(int i=0;i<MAX_BALLS;i++) balls[i].active=false;
  initBall(0,80,40,2.0,-2.0);
}

void drawHUD(){
  tft.fillRect(0,0,160,8,TFT_BLACK);
  tft.setTextColor(TFT_WHITE,TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(2,0); tft.print("Sc:"); tft.print(score);
  tft.setCursor(70,0); tft.print("Lv:"); tft.print(level);
  tft.setCursor(110,0); tft.print("L:"); tft.print(lives);
}

void showGameOver(){
  isGameOver=true;
  tft.fillRect(30,28,100,24,TFT_WHITE);
  tft.drawRect(30,28,100,24,TFT_BLACK);
  tft.setTextColor(TFT_BLACK,TFT_WHITE);
  tft.setCursor(45,34); tft.print("GAME OVER");
  tft.setCursor(38,44); tft.print("Press Btn to restart");
  tft.setTextColor(TFT_WHITE,TFT_BLACK);
}
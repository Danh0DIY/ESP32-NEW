/* Tetris Mini (ESP32-C3 + ST7735S 160x80 via TFT_eSPI)
   - Field: 10x20 cells, each cell 8x4 pixels => 80x80 at left side
   - Right side (80x80) for HUD: score, level, best, next piece
   - Buttons (INPUT_PULLUP, pressed = LOW):
       GPIO0: move left
       GPIO2: move right
       GPIO1: rotate CW (hold >=400ms for soft drop)
   - Hold LEFT+RIGHT ~600ms: pause/resume
   - On GAME OVER: press ROTATE to restart
   - High Score persisted with Preferences (NVS), ns="tetris", key="high"
*/

#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <Preferences.h>

TFT_eSPI tft;
Preferences prefs;

// ===== Buttons =====
#define BUTTON_LEFT   20
#define BUTTON_RIGHT  21
#define BUTTON_ROT    0

// ===== Field geometry =====
const int CELL_W = 8;
const int CELL_H = 4;
const int COLS   = 10;
const int ROWS   = 20;

// Field top-left pixel on screen (landscape 160x80)
const int FX = 0;
const int FY = 0;
// HUD area
const int HUD_X = 80;
const int HUD_Y = 0;
const int HUD_W = 80;
const int HUD_H = 80;

// ===== Colors =====
#define C_BG     TFT_BLACK
#define C_GRID   0x39E7
#define C_TEXT   TFT_WHITE
// 7 tetromino colors (I, J, L, O, S, T, Z)
uint16_t PCOL[7] = {
  TFT_CYAN, 0x001F, 0xFDA0, TFT_YELLOW, TFT_GREEN, TFT_MAGENTA, TFT_RED
};

// ===== Game state =====
int8_t field[ROWS][COLS]; // -1 empty, 0..6 color index
uint32_t score = 0;
uint16_t level = 1;
uint16_t linesCleared = 0;
uint32_t highScore = 0;
bool paused = false;
bool gameOver = false;

// Fall timing
uint32_t lastFall = 0;
uint16_t fallMs = 700;      // gravity (decreases with level)
const uint16_t minFallMs = 120;

// Input state
bool lastL = true, lastR = true, lastRot = true;
uint32_t bothHoldStart = 0;
const uint16_t bothHoldMs = 600;
uint32_t rotPressStart = 0; // for soft drop via long-press
const uint16_t softHoldMs = 400;

// ===== Tetromino definitions =====
const uint16_t SHAPES[7][4] = {
  // I
  { 0x0F00, 0x2222, 0x00F0, 0x4444 },
  // J
  { 0x8E00, 0x6440, 0x0E20, 0x44C0 },
  // L
  { 0x2E00, 0x4460, 0x0E80, 0xC440 },
  // O
  { 0x6600, 0x6600, 0x6600, 0x6600 },
  // S
  { 0x6C00, 0x4620, 0x06C0, 0x8C40 },
  // T
  { 0x4E00, 0x4640, 0x0E40, 0x4C40 },
  // Z
  { 0xC600, 0x2640, 0x0C60, 0x4C80 }
};

struct Piece {
  int type;      // 0..6
  int rot;       // 0..3
  int x, y;      // top-left of 4x4 mask in field coords
} cur, nxt;

// ===== Utils =====
bool btnL()  { return digitalRead(BUTTON_LEFT)  == LOW; }
bool btnR()  { return digitalRead(BUTTON_RIGHT) == LOW; }
bool btnRot(){ return digitalRead(BUTTON_ROT)   == LOW; }

void clearField() {
  for (int r=0;r<ROWS;++r) for (int c=0;c<COLS;++c) field[r][c] = -1;
}

void drawCellPx(int cx, int cy, uint16_t col) {
  int px = FX + cx*CELL_W;
  int py = FY + cy*CELL_H;
  tft.fillRect(px+1, py+1, CELL_W-2, CELL_H-2, col);
}

void drawGrid() {
  // Field background
  tft.fillRect(FX, FY, COLS*CELL_W, ROWS*CELL_H, C_BG);
  // Grid lines
  for (int x=0; x<=COLS; ++x)
    tft.drawFastVLine(FX + x*CELL_W, FY, ROWS*CELL_H, C_GRID);
  for (int y=0; y<=ROWS; ++y)
    tft.drawFastHLine(FX, FY + y*CELL_H, COLS*CELL_W, C_GRID);

  // HUD
  tft.fillRect(HUD_X, HUD_Y, HUD_W, HUD_H, C_BG);
  tft.drawRect(HUD_X, HUD_Y, HUD_W, HUD_H, C_GRID);
}

void drawHUD() {
  tft.setTextColor(C_TEXT, C_BG);
  tft.setTextSize(1);
  tft.setTextDatum(TL_DATUM);

  // Text lines
  tft.fillRect(HUD_X+2, HUD_Y+2, HUD_W-4, 40, C_BG);
  tft.setCursor(HUD_X+6, HUD_Y+4);
  tft.printf("Score:%lu", (unsigned long)score);
  tft.setCursor(HUD_X+6, HUD_Y+14);
  tft.printf("Level:%u", level);
  tft.setCursor(HUD_X+6, HUD_Y+24);
  tft.printf("Best:%lu", (unsigned long)highScore);
  tft.setCursor(HUD_X+6, HUD_Y+34);
  tft.printf(paused ? "[PAUSE]" : "        ");

  // Next preview box
  tft.drawRect(HUD_X+10, HUD_Y+48, 60, 28, C_GRID);
  // Clear inside
  tft.fillRect(HUD_X+11, HUD_Y+49, 58, 26, C_BG);

  // Draw next in 4x4 preview, centered
  uint16_t mask = SHAPES[nxt.type][0];
  int ox = HUD_X + 11 + (58 - 4*CELL_W)/2;
  int oy = HUD_Y + 49 + (26 - 4*CELL_H)/2;
  for (int i=0;i<16;++i) {
    if (mask & (0x8000 >> i)) {
      int cx = (i % 4);
      int cy = (i / 4);
      tft.fillRect(ox + cx*CELL_W +1, oy + cy*CELL_H +1, CELL_W-2, CELL_H-2, PCOL[nxt.type]);
    }
  }
}

bool checkCollision(const Piece& p, int nx, int ny, int nrot) {
  uint16_t mask = SHAPES[p.type][nrot & 3];
  for (int i=0;i<16;++i) {
    if (mask & (0x8000 >> i)) {
      int cx = nx + (i % 4);
      int cy = ny + (i / 4);
      if (cx < 0 || cx >= COLS || cy >= ROWS) return true; // wall/floor
      if (cy >= 0 && field[cy][cx] != -1) return true;     // block
    }
  }
  return false;
}

void drawPiece(const Piece& p, uint16_t color) {
  uint16_t mask = SHAPES[p.type][p.rot];
  for (int i=0;i<16;++i) {
    if (mask & (0x8000 >> i)) {
      int cx = p.x + (i % 4);
      int cy = p.y + (i / 4);
      if (cy >= 0) drawCellPx(cx, cy, color);
    }
  }
}

void lockPiece() {
  uint16_t mask = SHAPES[cur.type][cur.rot];
  for (int i=0;i<16;++i) {
    if (mask & (0x8000 >> i)) {
      int cx = cur.x + (i % 4);
      int cy = cur.y + (i / 4);
      if (cy >= 0 && cx>=0 && cx<COLS && cy<ROWS) field[cy][cx] = cur.type;
    }
  }
}

void persistHighScoreIfNeeded() {
  if (score > highScore) {
    highScore = score;
    prefs.putULong("high", highScore);
  }
}

void spawnPiece() {
  cur.type = nxt.type;
  cur.rot = 0;
  cur.x = 3;          // center-ish
  cur.y = -2;         // spawn above visible field
  // New next
  nxt.type = random(0,7);
  // If immediate collision when moving into field => game over
  if (checkCollision(cur, cur.x, cur.y+1, cur.rot)) {
    gameOver = true;
    persistHighScoreIfNeeded();
  }
}

void redrawField() {
  for (int r=0;r<ROWS;++r) for (int c=0;c<COLS;++c) {
    uint16_t col = (field[r][c] == -1) ? C_BG : PCOL[field[r][c]];
    drawCellPx(c, r, col);
  }
}

uint16_t clearLines() {
  uint16_t cleared = 0;
  for (int r=ROWS-1; r>=0; --r) {
    bool full = true;
    for (int c=0;c<COLS;++c) if (field[r][c] == -1) { full = false; break; }
    if (full) {
      cleared++;
      for (int rr=r; rr>0; --rr)
        for (int c=0;c<COLS;++c) field[rr][c] = field[rr-1][c];
      for (int c=0;c<COLS;++c) field[0][c] = -1;
      r++; // recheck same row after shift
    }
  }
  if (cleared) redrawField();
  return cleared;
}

void updateLevelSpeed() {
  level = 1 + linesCleared / 10;
  uint16_t base = 700;
  fallMs = base - (level-1) * 60;
  if (fallMs < minFallMs) fallMs = minFallMs;
}

void resetGame() {
  score = 0;
  level = 1;
  linesCleared = 0;
  paused = false;
  gameOver = false;
  clearField();
  drawGrid();
  nxt.type = random(0,7);
  spawnPiece();
  redrawField();
  drawHUD();
  lastFall = millis();
  updateLevelSpeed();
}

void gameOverScreen() {
  tft.fillRect(HUD_X+2, HUD_Y+2, HUD_W-4, HUD_H-4, C_BG);
  tft.setTextColor(TFT_RED, C_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(2);
  tft.drawString("GAME", HUD_X+HUD_W/2, HUD_Y+28);
  tft.drawString("OVER", HUD_X+HUD_W/2, HUD_Y+50);
  tft.setTextSize(1);
  tft.setTextColor(C_TEXT, C_BG);
  char buf[40];
  snprintf(buf, sizeof(buf), "Score:%lu  Best:%lu",
           (unsigned long)score, (unsigned long)highScore);
  tft.drawString(buf, HUD_X+HUD_W/2, HUD_Y+68);
}

void setup() {
  pinMode(BUTTON_LEFT,  INPUT_PULLUP);
  pinMode(BUTTON_RIGHT, INPUT_PULLUP);
  pinMode(BUTTON_ROT,   INPUT_PULLUP);

  randomSeed((uint32_t)analogRead(A0) ^ micros());

  tft.init();
  tft.setRotation(3); // landscape 160x80
  tft.fillScreen(C_BG);
  tft.setSwapBytes(true);

  // Preferences for High Score
  prefs.begin("tetris", false);
  highScore = prefs.getULong("high", 0);

  // preset last button states
  lastL = !btnL();
  lastR = !btnR();
  lastRot = !btnRot();

  resetGame();
}

void handleInput() {
  bool l = btnL();
  bool r = btnR();
  bool rot = btnRot();

  // Pause toggle: hold both left+right
  if (l && r) {
    if (bothHoldStart==0) bothHoldStart = millis();
    else if (!gameOver && millis()-bothHoldStart >= bothHoldMs) {
      paused = !paused;
      bothHoldStart = 0;
      drawHUD();
    }
  } else bothHoldStart = 0;

  if (!paused && !gameOver) {
    // Edge detect for left/right move
    if (l && !lastL) {
      if (!checkCollision(cur, cur.x-1, cur.y, cur.rot)) {
        drawPiece(cur, C_BG);
        cur.x--;
        drawPiece(cur, PCOL[cur.type]);
      }
    }
    if (r && !lastR) {
      if (!checkCollision(cur, cur.x+1, cur.y, cur.rot)) {
        drawPiece(cur, C_BG);
        cur.x++;
        drawPiece(cur, PCOL[cur.type]);
      }
    }
    // Rotate CW on press
    if (rot && !lastRot) {
      if (!checkCollision(cur, cur.x, cur.y, (cur.rot+1)&3)) {
        drawPiece(cur, C_BG);
        cur.rot = (cur.rot+1) & 3;
        drawPiece(cur, PCOL[cur.type]);
      } else {
        // simple wall kick: try x-1 or x+1
        if (!checkCollision(cur, cur.x-1, cur.y, (cur.rot+1)&3)) {
          drawPiece(cur, C_BG);
          cur.x -= 1; cur.rot = (cur.rot+1)&3;
          drawPiece(cur, PCOL[cur.type]);
        } else if (!checkCollision(cur, cur.x+1, cur.y, (cur.rot+1)&3)) {
          drawPiece(cur, C_BG);
          cur.x += 1; cur.rot = (cur.rot+1)&3;
          drawPiece(cur, PCOL[cur.type]);
        }
      }
      rotPressStart = millis(); // start soft drop timer too
    }
    // Soft drop if rotate is held
    if (rot && lastRot && (millis()-rotPressStart >= softHoldMs)) {
      static uint32_t lastSoft = 0;
      if (millis() - lastSoft >= 60) {
        lastSoft = millis();
        if (!checkCollision(cur, cur.x, cur.y+1, cur.rot)) {
          drawPiece(cur, C_BG);
          cur.y++;
          drawPiece(cur, PCOL[cur.type]);
        } else {
          // lock if cannot go further
          lockPiece();
          uint16_t got = clearLines();
          if (got) {
            linesCleared += got;
            const uint16_t tbl[5]={0,40,100,300,1200};
            score += tbl[got]*level;
            updateLevelSpeed();
            drawHUD();
          }
          spawnPiece();
          if (gameOver) {
            persistHighScoreIfNeeded();
            gameOverScreen();
          }
        }
      }
    }
  }

  // Restart on ROTATE press when game over
  if (gameOver && rot && !lastRot) {
    resetGame();
  }

  lastL = l; lastR = r; lastRot = rot;
}

void gravityStep() {
  if (paused || gameOver) return;
  uint32_t now = millis();
  if (now - lastFall < fallMs) return;
  lastFall = now;

  if (!checkCollision(cur, cur.x, cur.y+1, cur.rot)) {
    drawPiece(cur, C_BG);
    cur.y++;
    drawPiece(cur, PCOL[cur.type]);
  } else {
    // lock piece
    lockPiece();
    uint16_t got = clearLines();
    if (got) {
      linesCleared += got;
      const uint16_t tbl[5]={0,40,100,300,1200};
      score += tbl[got]*level;
      updateLevelSpeed();
      drawHUD();
    }
    spawnPiece();
    if (gameOver) {
      persistHighScoreIfNeeded();
      gameOverScreen();
    }
  }
}

void loop() {
  handleInput();
  gravityStep();
}
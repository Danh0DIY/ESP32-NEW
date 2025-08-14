#include <TFT_eSPI.h>
#include "FlappyBird.h"
#include "video_player.h"

TFT_eSPI tft;
FlappyBird game(tft, 23); // 0 = chạm nút nhảy (giữ nguyên từ code gốc)

const int SELECT_PIN = 13; // Chân để tùy chọn (toggle giữa các lựa chọn)
const int ENTER_PIN = 15;  // Chân để chọn và thoát

enum State { MENU, GAME, VIDEO };
State currentState = MENU;

int menuSelection = 0; // 0: Play Video, 1: Play Game

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  
  pinMode(SELECT_PIN, INPUT_PULLUP);
  pinMode(ENTER_PIN, INPUT_PULLUP);
  
  drawMenu();
}

void loop() {
  if (currentState == MENU) {
    handleMenu();
  } else if (currentState == GAME) {
    game.update();
    checkExit();
  } else if (currentState == VIDEO) {
    playVideos();
    checkExit();
  }
}

void drawMenu() {
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.drawString("Menu:", 10, 10);
  tft.drawString("1. Play Video", 10, 40);
  tft.drawString("2. Play Game", 10, 70);
  highlightSelection();
}

void highlightSelection() {
  // Xóa highlight cũ
  tft.fillRect(0, 40, 5, 60, TFT_BLACK);
  if (menuSelection == 0) {
    tft.fillRect(0, 40, 5, 30, TFT_RED); // Highlight cho Video
  } else {
    tft.fillRect(0, 70, 5, 30, TFT_RED); // Highlight cho Game
  }
}

void handleMenu() {
  static unsigned long lastDebounce = 0;
  if (millis() - lastDebounce > 200) { // Debounce 200ms
    if (digitalRead(SELECT_PIN) == LOW) {
      menuSelection = 1 - menuSelection; // Toggle giữa 0 và 1
      highlightSelection();
      lastDebounce = millis();
    }
    if (digitalRead(ENTER_PIN) == LOW) {
      lastDebounce = millis();
      if (menuSelection == 0) {
        currentState = VIDEO;
        tft.fillScreen(TFT_BLACK); // Xóa màn hình trước khi vào video
        // Nếu cần init video, thêm ở đây
      } else {
        currentState = GAME;
        tft.fillScreen(TFT_BLACK); // Xóa màn hình trước khi vào game
        game.begin();
      }
    }
  }
}

void checkExit() {
  static unsigned long lastDebounce = 0;
  if (millis() - lastDebounce > 200) {
    if (digitalRead(ENTER_PIN) == LOW) {
      currentState = MENU;
      tft.fillScreen(TFT_BLACK);
      drawMenu();
      lastDebounce = millis();
    }
  }
}
#include <TFT_eSPI.h>
#include "FlappyBird.h"

TFT_eSPI tft;
FlappyBird game(tft, 23); // 0 = chân nút nhấn

void setup() {
  tft.init();
  tft.setRotation(0);
  game.begin();
}

void loop() {
  game.update();
}
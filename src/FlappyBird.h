#ifndef FLAPPY_BIRD_H
#define FLAPPY_BIRD_H

#include <TFT_eSPI.h>
#include <SPI.h>

class FlappyBird {
public:
    FlappyBird(TFT_eSPI &display, int btnPin)
        : tft(display), BTN_PIN(btnPin) {}

    void begin() {
        pinMode(BTN_PIN, INPUT_PULLUP);
        tft.fillScreen(TFT_CYAN);
        randomSeed(analogRead(0));
        resetGame();
    }

    void update() {
        if (!gameOver) {
            // Xóa chim cũ
            tft.fillRect(birdX, oldBirdY, birdSize, birdSize, TFT_CYAN);
            // Xóa ống cũ
            drawPipe(oldPipeX, pipeTopHeight, TFT_CYAN);
            drawGround();

            // Nhảy
            if (digitalRead(BTN_PIN) == LOW) velocity = jumpStrength;

            // Cập nhật chim
            oldBirdY = birdY;
            velocity += gravity;
            birdY += velocity;

            // Cập nhật ống
            oldPipeX = pipeX;
            pipeX -= 2;
            if (pipeX + pipeWidth < 0) {
                pipeX = SCREEN_W;
                pipeTopHeight = random(20, SCREEN_H - pipeGap - 20);
                score++;
            }

            // Vẽ mới
            drawSprite(birdX, birdY, birdSprite, birdSize, birdSize);
            drawPipe(pipeX, pipeTopHeight, TFT_GREEN);
            drawGround();

            // Điểm
            tft.setTextColor(TFT_WHITE, TFT_CYAN);
            tft.setTextSize(2);
            tft.setCursor(5, 5);
            tft.print(score);

            // Va chạm
            if (birdY < 0 || birdY + birdSize > SCREEN_H - 16 ||
                (birdX + birdSize > pipeX && birdX < pipeX + pipeWidth &&
                 (birdY < pipeTopHeight || birdY + birdSize > pipeTopHeight + pipeGap))) {
                gameOver = true;
            }

            delay(20);
        } else {
            tft.setTextColor(TFT_RED, TFT_CYAN);
            tft.setTextSize(2);
            tft.setCursor(5, SCREEN_H / 2 - 10);
            tft.print("GAME OVER");
            tft.setCursor(5, SCREEN_H / 2 + 10);
            tft.print("Score:");
            tft.print(score);

            delay(1000);
            if (digitalRead(BTN_PIN) == LOW) resetGame();
        }
    }

private:
    TFT_eSPI &tft;
    int BTN_PIN;

    // Màn hình
    const int SCREEN_W = 80;
    const int SCREEN_H = 160;

    // Chim
    int birdX = 20;
    int birdY = 80;
    int birdSize = 8;
    float velocity = 0;
    float gravity = 0.4;
    float jumpStrength = -4.5;

    // Ống
    int pipeX = SCREEN_W;
    int pipeGap = 45;
    int pipeWidth = 15;
    int pipeTopHeight;

    // Game
    bool gameOver = false;
    int score = 0;
    int oldBirdY = birdY, oldPipeX = pipeX;

    // Sprite chim 8x8
    const uint16_t birdSprite[8*8] = {
        0xFFFF,0xFFFF,0xFFE0,0xFFE0,0xFFE0,0xFFFF,0xFFFF,0xFFFF,
        0xFFFF,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFFF,0xFFFF,
        0xFFFF,0xFFE0,0xFFFF,0xFFFF,0xFFFF,0xFFE0,0xFFE0,0xFFFF,
        0xFFFF,0xFFE0,0xFFFF,0x0000,0xFFFF,0xFD20,0xFFE0,0xFFFF,
        0xFFFF,0xFFE0,0xFFFF,0xFFFF,0xFFFF,0xFFE0,0xFFE0,0xFFFF,
        0xFFFF,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFE0,0xFFFF,0xFFFF,
        0xFFFF,0xFFFF,0xFFE0,0xFFE0,0xFFE0,0xFFFF,0xFFFF,0xFFFF,
        0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF,0xFFFF
    };

    void resetGame() {
        birdY = SCREEN_H / 2;
        velocity = 0;
        pipeX = SCREEN_W;
        pipeTopHeight = random(20, SCREEN_H - pipeGap - 20);
        score = 0;
        gameOver = false;
        tft.fillScreen(TFT_CYAN);
        drawGround();
    }

    void drawSprite(int x, int y, const uint16_t *sprite, int w, int h) {
        tft.pushImage(x, y, w, h, sprite);
    }

    void drawPipe(int x, int topH, uint16_t color) {
        // Ống trên
        tft.fillRect(x, 0, pipeWidth, topH, color);
        tft.drawRect(x, 0, pipeWidth, topH, TFT_DARKGREEN);
        // Ống dưới
        tft.fillRect(x, topH + pipeGap, pipeWidth, SCREEN_H - (topH + pipeGap) - 16, color);
        tft.drawRect(x, topH + pipeGap, pipeWidth, SCREEN_H - (topH + pipeGap) - 16, TFT_DARKGREEN);
    }

    void drawGround() {
        tft.fillRect(0, SCREEN_H - 16, SCREEN_W, 16, TFT_BROWN);
        tft.fillRect(0, SCREEN_H - 20, SCREEN_W, 4, TFT_GREEN);
    }
};

#endif
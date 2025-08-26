#include <Arduino.h>

void setup() {
  Serial.begin(115200);
}

void loop() {
  int hallValue = hallRead();  // Đọc giá trị từ cảm biến Hall tích hợp
  Serial.println(hallValue);
  delay(500);  // Đọc mỗi 0.5 giây
}
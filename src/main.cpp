// Cài đặt chân LED
const int ledPin = 8;

void setup() {
  pinMode(ledPin, OUTPUT); // Thiết lập chân 8 là OUTPUT
}

void loop() {
  digitalWrite(ledPin, HIGH); // Bật LED
  delay(500);                  // Chờ 500ms
  digitalWrite(ledPin, LOW);  // Tắt LED
  delay(500);                  // Chờ 500ms
}
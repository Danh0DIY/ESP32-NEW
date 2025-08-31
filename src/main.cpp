#include <Arduino.h>
#include <USB.h>
#include <USBHIDMouse.h>

USBHIDMouse Mouse;
const int buttonPin = 0;

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  USB.begin();
  Mouse.begin();
}

void loop() {
  if (digitalRead(buttonPin) == LOW) {
    Mouse.moveTo(616, 586);   // di chuột tới toạ độ
    Mouse.click(MOUSE_LEFT);  // click chuột
    delay(500);
  }
}
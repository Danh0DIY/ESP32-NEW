#include "USB.h"
#include "USBHIDMouse.h"

USBHIDMouse Mouse;
const int buttonPin = 0; // chân nút bấm (GPIO0)

void setup() {
  pinMode(buttonPin, INPUT_PULLUP);
  USB.begin();
  Mouse.begin();
  delay(2000);
}

void loop() {
  if (digitalRead(buttonPin) == LOW) { // nhấn nút
    Mouse.moveTo(980, 2200);
    Mouse.click(MOUSE_LEFT);
    delay(500); // chống dội nút
  }
}
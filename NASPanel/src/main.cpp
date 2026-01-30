#include <Arduino.h>

void setup() {
  pinMode(43, OUTPUT);
  pinMode(44, OUTPUT);

  digitalWrite(43, LOW);
  digitalWrite(44, LOW);
}

void loop() {
  static unsigned long lastToggleMs = 0;
  static bool level = false;

  const unsigned long now = millis();
  if (now - lastToggleMs >= 500) {
    lastToggleMs = now;
    level = !level;

    digitalWrite(43, level ? HIGH : LOW);
    digitalWrite(44, level ? HIGH : LOW);
  }
}
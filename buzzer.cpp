#include "buzzer.h"

#define BUZZER_PIN 23

void buzzerInit() {
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(BUZZER_PIN, LOW);

  ledcAttach(BUZZER_PIN, 2700, 8); 
}

void beep(uint16_t freq, uint16_t ms) {
  ledcWriteTone(BUZZER_PIN, freq);
  delay(ms);
  ledcWriteTone(BUZZER_PIN, 0);
}

void soundTaskAdded() {
  beep(2700, 60);
  delay(40);
  beep(3500, 60);
}

void soundError() {
  beep(3000, 150);
  delay(50);
  beep(3000, 250);
}

void soundOk() {
  beep(2700, 50);
}

void soundWarning() {
  beep(2400, 80);
  delay(40);
  beep(2400, 80);
}

void soundClick() {
  beep(4000, 15);
}

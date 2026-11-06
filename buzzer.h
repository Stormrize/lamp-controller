#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>

void buzzerInit();

void beep(uint16_t freq, uint16_t ms);

void soundTaskAdded();
void soundError();
void soundOk();
void soundWarning();
void soundClick();

#endif
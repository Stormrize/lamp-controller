#pragma once
#include <Arduino.h>
#include "scheduler.h"

// ── Choose ONE ────────────────────────────────────────────────────
#define USE_OLED          // 0.96" SSD1306, I2C addr 0x3C
// #define USE_LCD        // 1602 parallel (pin config in display.cpp)
// #define LCD_I2C        // add if you have I2C backpack (addr 0x27)
// ─────────────────────────────────────────────────────────────────

struct UIState {
  uint8_t  h, m, s;       // RTC time
  int      dim;           // brightness 0-255
  bool     on;            // lamp state
  bool     inputMode;
  uint8_t  inputBuf[7];
  int      inputLen;
};

void displayInit();
void displayUpdate(const UIState& ui);
// Timed popup: shows for `ms` ms then clears automatically
void displayMsg(const char* l1, const char* l2 = nullptr, uint32_t ms = 2000);
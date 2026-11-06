#include "display.h"
#include <stdarg.h>

#if defined(USE_OLED) && defined(USE_LCD)
  #error "Pick one: USE_OLED or USE_LCD"
#endif
#if !defined(USE_OLED) && !defined(USE_LCD)
  #error "Define USE_OLED or USE_LCD in display.h"
#endif

// ── Shared: timed popup message ───────────────────────────────────
static char     popL1[17], popL2[17];
static bool     popOn     = false;
static uint32_t popExpire = 0;

void displayMsg(const char* l1, const char* l2, uint32_t ms) {
  strncpy(popL1, l1 ? l1 : "", 16); popL1[16] = '\0';
  strncpy(popL2, l2 ? l2 : "", 16); popL2[16] = '\0';
  popOn     = true;
  popExpire = millis() + ms;
}

static bool popActive() {
  if (popOn && millis() > popExpire) popOn = false;
  return popOn;
}

// ══════════════════════════════════════════════════════════════════
// OLED  128×64  SSD1306
// ══════════════════════════════════════════════════════════════════
#ifdef USE_OLED
#include <Wire.h>
#include <Adafruit_SSD1306.h>

static Adafruit_SSD1306 oled(128, 64, &Wire, -1);

// Formatted print at (x, y)
static void op(int x, int y, const char* fmt, ...) {
  char b[22];
  va_list ap; va_start(ap, fmt);
  vsnprintf(b, sizeof(b), fmt, ap);
  va_end(ap);
  oled.setCursor(x, y);
  oled.print(b);
}

static void hline(int y) {
  oled.drawFastHLine(0, y, 128, SSD1306_WHITE);
}

// 60×6 brightness bar
static void bbar(int x, int y, int val) {
  oled.drawRect(x, y, 60, 6, SSD1306_WHITE);
  int f = map(val, 0, 255, 0, 58);
  if (f > 0) oled.fillRect(x+1, y+1, f, 4, SSD1306_WHITE);
}

void displayInit() {
  if (!oled.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED not found"); return;
  }
  oled.clearDisplay();
  oled.setTextColor(SSD1306_WHITE);
  oled.setTextSize(1);
  oled.setCursor(22, 28); oled.print("Lamp Controller");
  oled.display();
  delay(1000);
}

void displayUpdate(const UIState& ui) {
  oled.clearDisplay();
  oled.setTextSize(1);
  oled.setTextColor(SSD1306_WHITE);

  // ── Popup ─────────────────────────────────────────────────────
  if (popActive()) {
    oled.drawRect(0, 0, 128, 64, SSD1306_WHITE);
    oled.drawRect(3, 3, 122, 58, SSD1306_WHITE);
    int x1 = max(0, (128 - (int)strlen(popL1) * 6) / 2);
    int x2 = max(0, (128 - (int)strlen(popL2) * 6) / 2);
    int y1  = popL2[0] ? 18 : 28;
    oled.setCursor(x1, y1); oled.print(popL1);
    if (popL2[0]) { oled.setCursor(x2, 34); oled.print(popL2); }
    oled.display(); return;
  }

  // ── Input mode ────────────────────────────────────────────────
  if (ui.inputMode) {
    oled.setCursor(28, 0); oled.print("-- NEW TASK --");
    hline(10);

    // [C H H M M D D]  x/7
    oled.setCursor(0, 14); oled.print('[');
    for (int i = 0; i < 7; i++) {
      if (i) oled.print(' ');
      oled.print(i < ui.inputLen ? (char)('0' + ui.inputBuf[i]) : '_');
    }
    oled.print(']');
    op(100, 14, "%d/7", ui.inputLen);

    oled.setCursor(0, 24); oled.print("CMD  HH  MM  DUR");
    hline(34);

    oled.setCursor(0, 38); oled.print("1=ON    2=OFF");
    oled.setCursor(0, 48); oled.print("3=SUNRISE  4=SUNSET");
    oled.setCursor(0, 57); oled.print("* back  OK confirm");
    oled.display(); return;
  }

  // ── Normal view ───────────────────────────────────────────────
  // Row 0: time + status
  op( 0,  0, "%02d:%02d:%02d", ui.h, ui.m, ui.s);
  op(72,  0, ui.on ? "ON" : "OFF");
  if (isFadeActive()) op(110, 0, "~");

  // Row 1: brightness bar
  oled.setCursor(0, 10); oled.print('B');
  bbar(9, 10, ui.dim);
  op(72, 10, "%3d", ui.dim);

  hline(19);

  // Rows 2–4: task list
  // Each line: "[N] SUNRISE 06:00 30m"  (21 chars, fits 128px)
  int total = activeTaskCount();
  if (total == 0) {
    oled.setCursor(16, 32); oled.print("No tasks scheduled");
  } else {
    int y = 22, shown = 0;
    Task t;
    for (int i = 0; i < total && shown < 3; i++) {
      if (!getTask(i, t)) continue;
      char dur[5] = "    ";
      if ((t.type == TASK_SUNRISE || t.type == TASK_SUNSET) && t.durationMin < 1000)
        snprintf(dur, sizeof(dur), "%3um", t.durationMin);
      op(0, y, "[%d] %-7s %02d:%02d%s",
         shown, taskTypeName(t.type), t.hour, t.minute, dur);
      y += 9; shown++;
    }
    if (total > 3) op(0, y, "  +%d more...", total - 3);
  }

  // Bottom hint
  oled.setCursor(0, 56); oled.print("*=list  #=task  <=clr");
  oled.display();
}

#endif // USE_OLED


// ══════════════════════════════════════════════════════════════════
// LCD 1602  16×2
// ══════════════════════════════════════════════════════════════════
#ifdef USE_LCD

#ifdef LCD_I2C
  #include <LiquidCrystal_I2C.h>
  static LiquidCrystal_I2C lcd(0x27, 16, 2);
  #define LCD_SETUP() do { lcd.init(); lcd.backlight(); } while(0)
#else
  // Parallel 4-bit — safe ESP32 pins
  // RS   E   D4   D5   D6   D7
  //  25  26   27   14   33   32
  #include <LiquidCrystal.h>
  static LiquidCrystal lcd(25, 26, 27, 14, 33, 32);
  #define LCD_SETUP() lcd.begin(16, 2)
#endif

// Write exactly 16 chars to row r, padding with spaces
static void lrow(int r, const char* s) {
  lcd.setCursor(0, r);
  int n = strlen(s);
  for (int i = 0; i < 16; i++)
    lcd.print(i < n ? s[i] : ' ');
}

static int      taskCycle   = 0;
static uint32_t lastCycleMs = 0;

void displayInit() {
  LCD_SETUP();
  lrow(0, "  Lamp Control  ");
  lrow(1, "    Ready!      ");
  delay(1200);
  lcd.clear();
}

void displayUpdate(const UIState& ui) {
  // Popup
  if (popActive()) { lrow(0, popL1); lrow(1, popL2); return; }

  if (ui.inputMode) {
    // L1: input buffer  "3 0 6 _ _ _ _ "
    char l1[17];
    char* p = l1;
    for (int i = 0; i < 7; i++) {
      if (i) *p++ = ' ';
      *p++ = (i < ui.inputLen) ? ('0' + ui.inputBuf[i]) : '_';
    }
    *p = '\0';
    lrow(0, l1);
    // L2: progress + cancel hint
    char l2[17];
    snprintf(l2, sizeof(l2), "%d/7  #=cancel  ", ui.inputLen);
    lrow(1, l2);

  } else {
    // L1: "06:15 ON  B:200"
    char l1[17];
    snprintf(l1, sizeof(l1), "%02d:%02d %s B:%3d",
             ui.h, ui.m, ui.on ? "ON " : "OFF", ui.dim);
    lrow(0, l1);

    // L2: tasks, cycling every 3 s
    int total = activeTaskCount();
    if (total > 0 && millis() - lastCycleMs > 3000) {
      lastCycleMs = millis();
      taskCycle   = (taskCycle + 1) % total;
    }

    char l2[17];
    if (total == 0) {
      strncpy(l2, isFadeActive() ? "Fading...       "
                                 : "No tasks        ", 16);
      l2[16] = '\0';
    } else {
      Task t; getTask(taskCycle, t);
      // "[N]SUNRISE  06:00"  exactly 16 chars
      snprintf(l2, sizeof(l2), "[%d]%-7s %02d:%02d",
               taskCycle, taskTypeName(t.type), t.hour, t.minute);
    }
    lrow(1, l2);
  }
}

#endif // USE_LCD
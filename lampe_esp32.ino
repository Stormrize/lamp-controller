#include <IRremote.hpp>
#include <RTClib.h>
#include "scheduler.h"
#include "display.h"
#include "buzzer.h"

// ── Pins ──────────────────────────────────────────────────────────
#define ZC_PIN   18
#define DIM_PIN  19
#define IR_PIN    4

// ── IR codes ──────────────────────────────────────────────────────
#define IR_0     0xE619FF00
#define IR_1     0xBA45FF00
#define IR_2     0xB946FF00
#define IR_3     0xB847FF00
#define IR_4     0xBB44FF00
#define IR_5     0xBF40FF00
#define IR_6     0xBC43FF00
#define IR_7     0xF807FF00
#define IR_8     0xEA15FF00
#define IR_9     0xF609FF00
#define IR_STAR  0xE916FF00
#define IR_HASH  0xF20DFF00
#define IR_UP    0xE718FF00
#define IR_DOWN  0xAD52FF00
#define IR_LEFT  0xF708FF00
#define IR_RIGHT 0xA55AFF00
#define IR_OK    0xE31CFF00

// ── Dimmer state (extern in scheduler.h) ──────────────────────────
volatile int  dimLevel = 128;
volatile bool lampOn   = true;

// ── Hardware ──────────────────────────────────────────────────────
static RTC_DS3231  rtc;
static bool        rtcOk = false;
static hw_timer_t *dimTimer = NULL;
static volatile uint32_t lastZC = 0;

// ── IR parser ─────────────────────────────────────────────────────
static bool    inputMode = false;
static uint8_t inputBuf[7];
static int     inputLen  = 0;

// ── Display ───────────────────────────────────────────────────────
static UIState  uiState      = {};
static bool     displayDirty = true;
static uint32_t lastDispMs   = 0;


// Collect current system status and prepare
// a complete UIState structure for rendering.
static void fillUIState() {
  if (rtcOk) {
    DateTime now = rtc.now();
    uiState.h = now.hour();
    uiState.m = now.minute();
    uiState.s = now.second();
  }
  uiState.dim       = dimLevel;
  uiState.on        = lampOn;
  uiState.inputMode = inputMode;
  memcpy(uiState.inputBuf, inputBuf, 7);
  uiState.inputLen  = inputLen;
}

// Timer interrupt.
//
// Generates a short gate pulse that turns on
// the TRIAC for the current AC half-cycle.
void IRAM_ATTR fireTriac() {
  if (!lampOn) return;
  digitalWrite(DIM_PIN, HIGH);
  delayMicroseconds(50);
  digitalWrite(DIM_PIN, LOW);
}

// Zero-cross interrupt.
//
// Called every AC half-cycle.
// Calculates required firing delay based on
// current brightness and arms the timer.
void IRAM_ATTR zeroCross() {
  uint32_t t = micros();
  if (t - lastZC < 5000) return;
  lastZC = t;
  if (!lampOn) return;
  int d = map(255 - dimLevel, 0, 255, 1200, 11000);
  timerWrite(dimTimer, 0);
  timerAlarm(dimTimer, d, false, 0);
}

// Convert IR key code into a decimal digit.
//
// Returns:
//   0..9  - valid digit
//   -1    - not a numeric key
static int irToDigit(uint32_t code) {
  switch (code) {
    case IR_0: return 0; case IR_1: return 1; case IR_2: return 2;
    case IR_3: return 3; case IR_4: return 4; case IR_5: return 5;
    case IR_6: return 6; case IR_7: return 7; case IR_8: return 8;
    case IR_9: return 9; default:   return -1;
  }
}

// Print current task-entry buffer to Serial.
//
// Example:
//   [3 1 8 3 0 _ _] 5/7
static void printBuf() {
  Serial.print("  [");
  for (int i = 0; i < 7; i++) {
    if (i) Serial.print(' ');
    Serial.print(i < inputLen ? (char)('0' + inputBuf[i]) : '_');
  }
  Serial.printf("]  %d/7\n", inputLen);
}

// Parse and validate the 7-digit task command.
//
// Format:
//
//   C HH MM DD
//
// C:
//   1 = ON
//   2 = OFF
//   3 = SUNRISE
//   4 = SUNSET
//
// HH:
//   hour (00-23)
//
// MM:
//   minute (00-59)
//
// DD:
//   duration in minutes
//
// If validation succeeds, a new scheduler
// task is created.
static void commitInput() {
  if (inputLen != 7) {
    displayMsg("Error:", "Need 7 digits", 2000); return;
  }
  uint8_t  cmd = inputBuf[0];
  uint8_t  h   = inputBuf[1] * 10 + inputBuf[2];
  uint8_t  m   = inputBuf[3] * 10 + inputBuf[4];
  uint16_t dur = inputBuf[5] * 10 + inputBuf[6];

  if (cmd < 1 || cmd > 4) { 
    displayMsg("Error:", "CMD must be 1-4"); 
    return; 
  }
  if (h > 23) { 
    displayMsg("Error:", "Invalid hour");
    return; 
  }
  if (m > 59) { 
    displayMsg("Error:", "Invalid minute");  
    return; 
  }

  if (addTask((TaskType)cmd, h, m, dur)) {
    char l2[17];
    snprintf(l2, sizeof(l2), "%-7s %02d:%02d", taskTypeName((TaskType)cmd), h, m);
    displayMsg("Task added!", l2, 2000);
  } else {
    displayMsg("List is full!", nullptr, 2000);
  }
}

// Main IR remote command handler.
//
// Supports:
//
// Normal mode:
//   UP     - increase brightness
//   DOWN   - decrease brightness
//   OK     - toggle lamp
//   *      - print task list
//   #      - enter task-input mode
//   LEFT   - clear all tasks
//   RIGHT  - show RTC time
//
// Input mode:
//   digits - enter task data
//   *      - backspace
//   OK     - save task
//   #      - cancel
static void handleIR(uint32_t code) {

  if (inputMode) {
    switch (code) {
      case IR_HASH:
        inputMode = false; inputLen = 0;
        Serial.println("Cancelled.");
        break;
      case IR_STAR:
        if (inputLen > 0) inputLen--;
        printBuf();
        break;
      case IR_OK:
        printBuf();
        commitInput();
        inputMode = false; inputLen = 0;
        break;
      default: {
        int d = irToDigit(code);
        if (d >= 0 && inputLen < 7) {
          inputBuf[inputLen++] = (uint8_t)d;
          printBuf();
        }
      }
    }
    displayDirty = true;
    return;
  }

  switch (code) {
    case IR_HASH:
      inputMode = true; inputLen = 0;
      Serial.println("New task  →  CMD H1H2 M1M2 D1D2  OK");
      Serial.println("  1=ON  2=OFF  3=Sunrise  4=Sunset");
      break;
    case IR_UP:
      cancelFade();
      dimLevel = min((int)dimLevel + 15, 255);
      lampOn   = true;
      Serial.printf("Bright+: %d\n", (int)dimLevel);
      break;
    case IR_DOWN:
      cancelFade();
      dimLevel = max((int)dimLevel - 15, 0);
      lampOn   = (dimLevel > 0);
      Serial.printf("Bright-: %d\n", (int)dimLevel);
      break;
    case IR_OK:
      lampOn = !lampOn;
      Serial.printf("Lamp %s\n", lampOn ? "ON" : "OFF");
      break;
    case IR_STAR:
      printTasks();
      break;
    case IR_LEFT:
      clearTasks();
      displayMsg("All tasks", "cleared", 1500);
      break;
    case IR_RIGHT:
      if (rtcOk) {
        DateTime now = rtc.now();
        Serial.printf("Time: %02d:%02d:%02d\n", now.hour(), now.minute(), now.second());
      }
      break;
  }
  displayDirty = true;
}

// System initialization.
//
// Initializes:
//   - Serial port
//   - RTC
//   - TRIAC dimmer
//   - Zero-cross detection
//   - Hardware timer
//   - IR receiver
//   - Display
void setup() {
  Serial.begin(9600);

  rtcOk = rtc.begin();
  if (!rtcOk) Serial.println("RTC not found – scheduling disabled");
  else if (rtc.lostPower()) {
    Serial.println("RTC set to compile time");
  }

  pinMode(ZC_PIN, INPUT);
  pinMode(DIM_PIN, OUTPUT);
  digitalWrite(DIM_PIN, LOW);
  attachInterrupt(digitalPinToInterrupt(ZC_PIN), zeroCross, RISING);
  dimTimer = timerBegin(1000000);
  timerAttachInterrupt(dimTimer, &fireTriac);

  IrReceiver.begin(IR_PIN, ENABLE_LED_FEEDBACK);
  displayInit();

  Serial.println("Ready.");
}

// Main application loop.
//
// Responsibilities:
//
// 1. Process IR remote commands.
// 2. Execute scheduled tasks.
// 3. Update active sunrise/sunset fades.
// 4. Refresh display when needed.
void loop() {
  // IR
  if (IrReceiver.decode()) {
    uint32_t code     = IrReceiver.decodedIRData.decodedRawData;
    bool     isRepeat = IrReceiver.decodedIRData.flags & IRDATA_FLAGS_IS_REPEAT;
    if (code != 0 && (!isRepeat || code == IR_UP || code == IR_DOWN))
      handleIR(code);
    IrReceiver.resume();
  }

  // Scheduler (every second)
  if (rtcOk) {
    static uint32_t lastCheck = 0;
    if (millis() - lastCheck >= 1000) {
      lastCheck = millis();
      checkTasks(rtc.now());
    }
  }

  updateFade();

  // Display (500 ms or on dirty flag)
  if (displayDirty || millis() - lastDispMs >= 500) {
    lastDispMs   = millis();
    displayDirty = false;
    fillUIState();
    displayUpdate(uiState);
  }
}
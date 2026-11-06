#pragma once

#include <Arduino.h>
#include <RTClib.h>


// These variables are defined in main.ino and control
// the actual lamp state.
//
// dimLevel:
//   Brightness level from 0 to 255.
//
// lampOn:
//   Master on/off state of the lamp.
extern volatile int  dimLevel;
extern volatile bool lampOn;


// Maximum number of scheduled tasks that can be stored.
#define MAX_TASKS 8


// Types of actions that the scheduler can perform.
enum TaskType : uint8_t {

  // Turn lamp on immediately.
  TASK_ON = 1,

  // Turn lamp off immediately.
  TASK_OFF = 2,

  // Simulate sunrise:
  // brightness gradually increases from 0 to maximum.
  TASK_SUNRISE = 3,

  // Simulate sunset:
  // brightness gradually decreases to zero.
  TASK_SUNSET = 4
};


// Represents a single scheduled task.
struct Task {

  // True if this task slot is currently used.
  bool active;

  // Action to execute.
  TaskType type;

  // Scheduled execution time (hour).
  uint8_t hour;

  // Scheduled execution time (minute).
  uint8_t minute;

  // Duration in minutes for sunrise/sunset effects.
  // Ignored for simple ON/OFF tasks.
  uint16_t durationMin;

  // Internal flag used to prevent multiple executions
  // during the same minute.
  bool firedThisMinute;
};


// ------------------------------------------------------------------
// Scheduler management
// ------------------------------------------------------------------

// Add a new task to the scheduler.
//
// Parameters:
//   type         - action type
//   hour         - execution hour
//   minute       - execution minute
//   durationMin  - fade duration for sunrise/sunset
//
// Returns true if task was added successfully.
bool addTask(
    TaskType type,
    uint8_t hour,
    uint8_t minute,
    uint16_t durationMin
);


// Remove all scheduled tasks and stop active fades.
void clearTasks();


// Check current time and execute matching tasks.
//
// Should be called periodically from loop().
void checkTasks(const DateTime& now);


// Update active sunrise/sunset transition.
//
// Should be called continuously from loop().
void updateFade();


// Stop active brightness transition immediately.
void cancelFade();


// Print all active tasks to Serial Monitor.
void printTasks();


// ------------------------------------------------------------------
// Read-only access helpers
// Useful for displays, web interfaces, and debugging.
// ------------------------------------------------------------------

// Return human-readable task type name.
const char* taskTypeName(TaskType t);


// Return number of currently active tasks.
int activeTaskCount();


// Get task by logical index.
//
// Returns true if task exists.
bool getTask(int logicalIdx, Task& out);


// Returns true if a sunrise/sunset transition
// is currently running.
bool isFadeActive();в
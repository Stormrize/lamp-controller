#include "scheduler.h"

// Fixed-size array that stores all scheduled tasks.
// Empty slots have active == false.
static Task tasks[MAX_TASKS] = {};

// Structure describing an active brightness transition
// (sunrise or sunset effect).
struct FadeState {
  bool     active;      // Is fade currently running?
  uint32_t startMs;     // millis() value when fade started
  uint32_t durationMs;  // Total fade duration in milliseconds
  int      fromLevel;   // Starting brightness (0-255)
  int      toLevel;     // Target brightness (0-255)
};

// Global fade controller.
static FadeState fade = {};


// Convert task type enum into a readable text string.
static const char* taskName(TaskType t) {
  switch (t) {
    case TASK_ON:      return "ON";
    case TASK_OFF:     return "OFF";
    case TASK_SUNRISE: return "SUNRISE";
    case TASK_SUNSET:  return "SUNSET";
    default:           return "?";
  }
}


// Add a new scheduled task.
//
// Parameters:
//   type         - task type (ON, OFF, SUNRISE, SUNSET)
//   hour         - execution hour
//   minute       - execution minute
//   durationMin  - duration for sunrise/sunset effects
//
// Returns true if task was added successfully.
bool addTask(TaskType type, uint8_t hour, uint8_t minute, uint16_t durationMin) {
  for (int i = 0; i < MAX_TASKS; i++) {

    // Find first free slot.
    if (!tasks[i].active) {

      // Create and store the task.
      tasks[i] = {true, type, hour, minute, durationMin, false};

      Serial.printf("[Task %d] %s at %02d:%02d, duration=%u min\n",
                    i, taskName(type), hour, minute, durationMin);

      return true;
    }
  }

  Serial.println("Task list is full!");
  return false;
}


// Remove all scheduled tasks and stop any active fade.
void clearTasks() {
  memset(tasks, 0, sizeof(tasks));
  fade.active = false;

  Serial.println("All tasks removed.");
}


// Check whether any task should be executed at current time.
//
// Called periodically (usually every loop iteration).
void checkTasks(const DateTime& now) {
  uint8_t h = now.hour();
  uint8_t m = now.minute();

  for (int i = 0; i < MAX_TASKS; i++) {

    if (!tasks[i].active)
      continue;

    // Task should trigger if current time matches its schedule.
    bool match = (tasks[i].hour == h &&
                  tasks[i].minute == m);

    // Prevent repeated execution during the same minute.
    if (match && !tasks[i].firedThisMinute) {

      tasks[i].firedThisMinute = true;

      Serial.printf("[Trigger] %s\n",
                    taskName(tasks[i].type));

      switch (tasks[i].type) {

        // Turn lamp on with default brightness.
        case TASK_ON:
          lampOn   = true;
          dimLevel = 128;
          break;

        // Turn lamp completely off.
        case TASK_OFF:
          lampOn = false;
          break;

        // Sunrise effect:
        // start from darkness and slowly increase brightness.
        case TASK_SUNRISE:
          lampOn   = true;
          dimLevel = 0;

          fade = {
            true,
            millis(),
            (uint32_t)tasks[i].durationMin * 60000UL,
            0,
            255
          };
          break;

        // Sunset effect:
        // start bright and slowly dim to zero.
        case TASK_SUNSET:
          lampOn   = true;
          dimLevel = 255;

          fade = {
            true,
            millis(),
            (uint32_t)tasks[i].durationMin * 60000UL,
            255,
            0
          };
          break;
      }
    }

    // Reset trigger flag after leaving scheduled minute.
    // This allows the task to run again on the next day.
    if (!match)
      tasks[i].firedThisMinute = false;
  }
}


// Update brightness during an active fade.
//
// Should be called continuously from loop().
void updateFade() {

  if (!fade.active)
    return;

  uint32_t elapsed = millis() - fade.startMs;

  // Fade completed.
  if (elapsed >= fade.durationMs) {

    dimLevel = fade.toLevel;

    // If final brightness is zero, switch lamp off.
    if (fade.toLevel <= 0)
      lampOn = false;

    fade.active = false;

    Serial.println("[Fade] Completed.");
    return;
  }

  // Progress value from 0.0 to 1.0.
  float t = (float)elapsed /
            (float)fade.durationMs;

  // Linear interpolation between start and end brightness.
  dimLevel =
      (int)(fade.fromLevel +
      t * (float)(fade.toLevel - fade.fromLevel));
}


// Stop current fade immediately.
void cancelFade() {
  if (fade.active) {
    fade.active = false;
    Serial.println("[Fade] Cancelled.");
  }
}


// Print all active tasks to Serial Monitor.
void printTasks() {

  Serial.println("─── Scheduled Tasks ───");

  bool any = false;

  for (int i = 0; i < MAX_TASKS; i++) {

    if (!tasks[i].active)
      continue;

    any = true;

    Serial.printf(
      "  [%d] %-7s %02d:%02d dur=%u min\n",
      i,
      taskName(tasks[i].type),
      tasks[i].hour,
      tasks[i].minute,
      tasks[i].durationMin
    );
  }

  if (!any)
    Serial.println("  (none)");
}


// ------------------------------------------------------------------
// Display helper functions
// These functions provide read-only access to scheduler data.
// Useful for OLED/LCD displays, web UI, etc.
// ------------------------------------------------------------------

// Return readable task type name.
const char* taskTypeName(TaskType t) {
  return taskName(t);
}


// Count how many active tasks currently exist.
int activeTaskCount() {
  int n = 0;

  for (int i = 0; i < MAX_TASKS; i++) {
    if (tasks[i].active)
      n++;
  }

  return n;
}


// Get task by logical index.
//
// Example:
// If active tasks are stored in slots 0, 4, and 8,
// logical indexes will be:
//   0 -> slot 0
//   1 -> slot 4
//   2 -> slot 8
//
// Returns true if task exists.
bool getTask(int logicalIdx, Task& out) {

  int found = 0;

  for (int i = 0; i < MAX_TASKS; i++) {

    if (!tasks[i].active)
      continue;

    if (found++ == logicalIdx) {
      out = tasks[i];
      return true;
    }
  }

  return false;
}


// Returns true if sunrise/sunset transition is running.
bool isFadeActive() {
  return fade.active;
}
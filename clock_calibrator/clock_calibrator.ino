#include <Wire.h>
#include "RTClib.h"

RTC_DS3231 rtc;

void setup() {
  Serial.begin(9600);

  // Initialize the RTC module
  if (!rtc.begin()) {
    Serial.println("Could not find DS3231 RTC module!");
    while (1);
  }

  // CHOOSE ONE OF THE METHODS BELOW TO SET THE TIME:

  // Method 1: Automatically sync to your computer's exact system time at compilation
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)) + TimeSpan(0, 0, 0, 10));

  // Method 2: Manually override with a specific date and time 
  // Format: DateTime(Year, Month, Day, Hour, Minute, Second)
  // rtc.adjust(DateTime(2026, 6, 4, 21, 30, 0)); 
}

void loop() {
  DateTime now = rtc.now();

  // Print current time to Serial Monitor
  Serial.print(now.year(), DEC);
  Serial.print('/');
  Serial.print(now.month(), DEC);
  Serial.print('/');
  Serial.print(now.day(), DEC);
  Serial.print(" ");
  Serial.print(now.hour(), DEC);
  Serial.print(':');
  Serial.print(now.minute(), DEC);
  Serial.print(':');
  Serial.print(now.second(), DEC);
  Serial.println();

  delay(1000); // Update once per second
}

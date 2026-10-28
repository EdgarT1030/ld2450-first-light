// ld2450-first-light.ino — Phase 1: Tracker v1
//
// Reads HLK-LD2450 radar frames on Serial2, assigns persistent track IDs (A/B/C)
// via proximity matching, and prints live track state to the USB Serial Monitor.
//
// ⚠️  NONSTANDARD CABLE COLORS — wire by PCB label, never by color:
//     Sensor 5V  (BLACK wire)  → ESP32 VIN
//     Sensor GND (YELLOW wire) → ESP32 GND
//     Sensor TX  (WHITE wire)  → ESP32 GPIO 17  (used as Serial2 RX)
//     Sensor RX  (RED wire)    → ESP32 GPIO 16  (used as Serial2 TX)
//     TX/RX cross over intentionally.
//
// Arduino IDE board: ESP32 Dev Module, 240 MHz, 4 MB flash, Default partitions.
// Serial Monitor:    115200 baud.
// Tuning constants:  config.h
// Debug/CSV flags:   config.h

#include "config.h"
#include "parser.h"
#include "tracker.h"

void setup() {
  Serial.begin(115200);
  // RX pin = GPIO 17 (white wire / sensor TX), TX pin = GPIO 16 (red wire / sensor RX)
  Serial2.begin(256000, SERIAL_8N1, 17, 16);
  trackerInit();

  Serial.println("LD2450 Tracker v1 ready — waiting for targets...");
#if DEBUG_CSV
  trackerPrintCsvHeader();
#endif
#if DEBUG_SLOTS
  Serial.println("[DEBUG_SLOTS] Raw slot output only — tracker disabled.");
#endif
}

void loop() {
  if (!syncHeader()) return;
  if (!parseFrame()) return;

#if DEBUG_SLOTS
  // Original Phase 0 behavior — raw slot output, no tracker.
  bool any = false;
  for (int t = 0; t < 3; t++) {
    if (rawTargets[t].res == 0) continue;
    any = true;
    Serial.print("Target "); Serial.print(t + 1);
    Serial.print(": x=");    Serial.print(rawTargets[t].x);
    Serial.print("mm  y=");  Serial.print(rawTargets[t].y);
    Serial.print("mm  speed="); Serial.print(rawTargets[t].speed);
    Serial.println("cm/s");
  }
  if (!any) Serial.println("(no targets in frame)");
  Serial.println("---");
  return;
#endif

  trackerUpdate(rawTargets);
  trackerPrint();
}

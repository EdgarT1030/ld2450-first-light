// parser.h — LD2450 frame sync and raw target parsing.
//
// Exposes:
//   rawTargets[3]  — filled by parseFrame() each call; slots with res==0 are empty
//   syncHeader()   — blocks until AA FF 03 00 found, or returns false on 200 ms silence
//   parseFrame()   — reads the 26-byte body after the header; returns true on success
//
// ⚠️  NONSTANDARD CABLE COLORS on this kit — wire by PCB label, never by color:
//     Sensor 5V  (BLACK wire)  → ESP32 VIN
//     Sensor GND (YELLOW wire) → ESP32 GND
//     Sensor TX  (WHITE wire)  → ESP32 GPIO 17  (used as RX in Serial2)
//     Sensor RX  (RED wire)    → ESP32 GPIO 16  (used as TX in Serial2)
//     TX and RX cross over intentionally.

#pragma once
#include <Arduino.h>
#include "config.h"

// ── Protocol constants ────────────────────────────────────────────────────────

static const uint8_t FRAME_HEADER[]  = {0xAA, 0xFF, 0x03, 0x00};
static const uint8_t FRAME_TAIL[]    = {0x55, 0xCC};
static const int     FRAME_BODY_LEN  = 26;  // 3 × 8-byte target slots + 2-byte tail

// ── Raw target (one sensor slot) ─────────────────────────────────────────────

struct RawTarget {
  int16_t  x;     // mm, positive = right of sensor
  int16_t  y;     // mm, positive = in front of sensor
  int16_t  speed; // cm/s, positive = moving away from sensor
  uint16_t res;   // resolution in mm; 0 = slot is empty, no target
};

// Global array filled by parseFrame() each call.
RawTarget rawTargets[3];

// ── Sign-magnitude decode ─────────────────────────────────────────────────────
// The LD2450 uses sign-magnitude encoding, NOT two's complement.
// Bit 15 is the sign flag: 1 = negative, 0 = positive.
// Treating raw bytes as a plain int16_t mirrors every target to the wrong side.
static inline int16_t signMag(uint16_t raw) {
  return (raw & 0x8000) ? -(int16_t)(raw & 0x7FFF) : (int16_t)raw;
}

// ── Frame sync ────────────────────────────────────────────────────────────────
// Reads bytes one at a time until the 4-byte header AA FF 03 00 is found.
// Returns false if no bytes arrive for 200 ms (sensor not ready yet).
static bool syncHeader() {
  int matched = 0;
  unsigned long lastByte = millis();

  while (matched < 4) {
    if (!Serial2.available()) {
      if (millis() - lastByte > 200) return false;
      continue;
    }
    lastByte = millis();
    uint8_t b = Serial2.read();
    matched = (b == FRAME_HEADER[matched]) ? matched + 1
                                           : (b == FRAME_HEADER[0] ? 1 : 0);
  }
  return true;
}

// ── Frame body parse ──────────────────────────────────────────────────────────
// Reads the 26 bytes following the header: 3 target slots × 8 bytes + 2-byte tail.
// Fills rawTargets[]. Returns false and prints an error if the frame is corrupt.
static bool parseFrame() {
  uint8_t body[FRAME_BODY_LEN];
  int received = 0;
  unsigned long deadline = millis() + 50;  // generous at 256 kbaud

  while (received < FRAME_BODY_LEN) {
    if (millis() > deadline) {
      Serial.println("[ERR] frame body timeout — dropped");
      return false;
    }
    if (Serial2.available()) body[received++] = Serial2.read();
  }

  if (body[FRAME_BODY_LEN - 2] != FRAME_TAIL[0] ||
      body[FRAME_BODY_LEN - 1] != FRAME_TAIL[1]) {
    Serial.println("[ERR] bad tail bytes — frame dropped");
    return false;
  }

#if DEBUG_RAW
  Serial.print("[RAW] AA FF 03 00 ");
  for (int i = 0; i < FRAME_BODY_LEN; i++) {
    if (body[i] < 0x10) Serial.print('0');
    Serial.print(body[i], HEX);
    Serial.print(' ');
  }
  Serial.println();
#endif

  // Each slot: X(2B) Y(2B) speed(2B) res(2B), all little-endian.
  for (int t = 0; t < 3; t++) {
    const uint8_t* s = body + t * 8;
    rawTargets[t].x     = signMag((uint16_t)s[0] | ((uint16_t)s[1] << 8));
    rawTargets[t].y     = -signMag((uint16_t)s[2] | ((uint16_t)s[3] << 8));  // sensor bit-15 convention is inverted for Y
    rawTargets[t].speed = signMag((uint16_t)s[4] | ((uint16_t)s[5] << 8));
    rawTargets[t].res   =         (uint16_t)s[6] | ((uint16_t)s[7] << 8);
  }
  return true;
}

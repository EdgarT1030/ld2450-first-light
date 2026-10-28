// tracker.h — persistent track management on top of LD2450 raw slots.
//
// The sensor reports up to 3 target slots, but slot identity is NOT stable
// frame to frame — slot 1 now can be slot 2 next frame.  This module assigns
// stable IDs (A / B / C) by nearest-neighbour proximity matching, applies EMA
// smoothing, and manages track birth, coast, and deletion.
//
// Usage:
//   trackerInit()              — call once in setup()
//   trackerUpdate(rawTargets)  — call every frame after parseFrame()
//   trackerPrint()             — call every frame (or less often) to emit Serial output
//
// Outputs are in tracks[].  Later phases (TFT, audio) read tracks[] directly.

#pragma once
#include <Arduino.h>
#include <math.h>
#include "config.h"
#include "parser.h"

// ── Track structure ───────────────────────────────────────────────────────────

struct Track {
  bool     active;        // slot is in use (includes the coast window)
  bool     confirmed;     // false until CONFIRM_FRAMES consecutive matches
  char     id;            // 'A', 'B', or 'C'
  float    x, y;          // smoothed position (mm)
  float    speed;         // smoothed speed magnitude (cm/s)
  uint32_t firstSeen;     // millis() when this slot was first created
  uint32_t lastSeen;      // millis() when last matched to a raw measurement
  uint32_t lastMoved;     // millis() when speed last exceeded STILL_SPEED_CM
  int      confirmCount;  // consecutive matched frames so far
};

Track tracks[3];

// ── Init ─────────────────────────────────────────────────────────────────────

void trackerInit() {
  for (int i = 0; i < 3; i++) {
    tracks[i].active    = false;
    tracks[i].confirmed = false;
    tracks[i].id        = 'A' + i;
  }
}

// ── Internal helpers ──────────────────────────────────────────────────────────

static float _dist(float x1, float y1, float x2, float y2) {
  float dx = x1 - x2, dy = y1 - y2;
  return sqrtf(dx * dx + dy * dy);
}

static int _freeSlot() {
  for (int i = 0; i < 3; i++)
    if (!tracks[i].active) return i;
  return -1;  // shortcut: drops a new arrival when all 3 slots are occupied by older tracks
              // upgrade if 4-target firmware ever ships
}

// ── Update: call once per parsed frame ───────────────────────────────────────

void trackerUpdate(RawTarget* raw) {
  uint32_t now = millis();
  bool matched[3] = {false, false, false};

  // Pass 1 — match each live raw measurement to the nearest unmatched track.
  for (int r = 0; r < 3; r++) {
    if (raw[r].res == 0) continue;  // empty sensor slot

    float rx = (float)raw[r].x;
    float ry = (float)raw[r].y;

    int   bestTrack = -1;
    float bestDist  = TRACKER_MAX_JUMP_MM;

    for (int t = 0; t < 3; t++) {
      if (!tracks[t].active || matched[t]) continue;
      float d = _dist(rx, ry, tracks[t].x, tracks[t].y);
      if (d < bestDist) { bestDist = d; bestTrack = t; }
    }

    if (bestTrack >= 0) {
      // Update existing track with EMA smoothing.
      Track& tr = tracks[bestTrack];
      tr.x     = TRACKER_EMA_ALPHA * rx      + (1.0f - TRACKER_EMA_ALPHA) * tr.x;
      tr.y     = TRACKER_EMA_ALPHA * ry      + (1.0f - TRACKER_EMA_ALPHA) * tr.y;
      float sp = fabsf((float)raw[r].speed);
      tr.speed = TRACKER_EMA_ALPHA * sp      + (1.0f - TRACKER_EMA_ALPHA) * tr.speed;
      tr.lastSeen = now;
      matched[bestTrack] = true;

      if (sp > TRACKER_STILL_SPEED_CM) tr.lastMoved = now;

      if (!tr.confirmed) {
        tr.confirmCount++;
        if (tr.confirmCount >= TRACKER_CONFIRM_FRAMES) tr.confirmed = true;
      }

    } else {
      // No close-enough existing track — start a new one.
      int slot = _freeSlot();
      if (slot >= 0) {
        Track& tr      = tracks[slot];
        tr.active       = true;
        tr.confirmed    = false;
        tr.x            = rx;
        tr.y            = ry;
        tr.speed        = fabsf((float)raw[r].speed);
        tr.firstSeen    = now;
        tr.lastSeen     = now;
        tr.lastMoved    = now;
        tr.confirmCount = 1;
      }
    }
  }

  // Pass 2 — expire tracks that have not been matched within the coast window.
  for (int t = 0; t < 3; t++) {
    if (tracks[t].active && !matched[t] &&
        (now - tracks[t].lastSeen > TRACKER_COAST_MS)) {
      tracks[t].active       = false;
      tracks[t].confirmed    = false;
      tracks[t].confirmCount = 0;
    }
  }
}

// ── Print: call each frame (or on a timer) to emit track state to Serial ──────

#if DEBUG_CSV
void trackerPrintCsvHeader() {
  Serial.println("CSV,millis,id,x_mm,y_mm,dist_mm,angle_deg,speed_cms,still_s");
}
#endif

void trackerPrint() {
  uint32_t now = millis();
  bool any = false;

  for (int t = 0; t < 3; t++) {
    if (!tracks[t].active || !tracks[t].confirmed) continue;
    any = true;

    Track& tr   = tracks[t];
    float dist  = sqrtf(tr.x * tr.x + tr.y * tr.y);
    // angle: 0° = straight ahead (+Y), positive = right, negative = left
    float angle = atan2f(tr.x, tr.y) * 180.0f / (float)M_PI;
    float stillS = (float)(now - tr.lastMoved) / 1000.0f;

#if DEBUG_CSV
    Serial.print("CSV,");
    Serial.print(now);              Serial.print(',');
    Serial.print(tr.id);            Serial.print(',');
    Serial.print((int)tr.x);        Serial.print(',');
    Serial.print((int)tr.y);        Serial.print(',');
    Serial.print((int)dist);        Serial.print(',');
    Serial.print(angle,  1);        Serial.print(',');
    Serial.print(tr.speed, 1);      Serial.print(',');
    Serial.println(stillS, 1);
#else
    Serial.print("[TRACK] ");
    Serial.print(tr.id);
    Serial.print("  x=");     Serial.print((int)tr.x);
    Serial.print("mm  y=");   Serial.print((int)tr.y);
    Serial.print("mm  dist=");Serial.print((int)dist);
    Serial.print("mm  angle=");Serial.print(angle, 1);
    Serial.print("deg  spd="); Serial.print(tr.speed, 1);
    Serial.print("cm/s  still="); Serial.print(stillS, 1);
    if ((now - tr.lastMoved) > TRACKER_STILL_MS) {
      Serial.println("s  [STILL]");
    } else {
      Serial.println("s  [MOVING]");
    }
#endif
  }

#if !DEBUG_CSV
  if (!any) Serial.println("(no confirmed tracks)");
  Serial.println("---");
#endif
}

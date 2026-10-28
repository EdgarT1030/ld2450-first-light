// config.h — debug flags and tracker tuning constants.
//
// Change values here; do not scatter magic numbers through the code.
// All distances in mm, times in ms, speeds in cm/s.

#pragma once

// ── Debug flags ───────────────────────────────────────────────────────────────

// 1 = print every raw 30-byte frame as hex (protocol debugging).
#define DEBUG_RAW   0

// 1 = print raw slot output only, skipping the tracker entirely.
// This restores the original Phase 0 behavior for side-by-side comparison.
#define DEBUG_SLOTS 0

// 1 = emit CSV lines instead of human-readable tracker output.
// Pipe Serial Monitor to a file for calibration data collection.
// CSV format: CSV,millis,id,x_mm,y_mm,dist_mm,angle_deg,speed_cms,still_s
#define DEBUG_CSV   0

// ── Tracker tuning ────────────────────────────────────────────────────────────

// Max mm a track may jump frame-to-frame before we stop treating it as the same target.
// ~500 mm = ~5 m/s at 10 fps — a fast run. Raise if IDs swap during quick movement.
#define TRACKER_MAX_JUMP_MM    500.0f

// How long (ms) to keep a track alive after its last matched measurement.
// During this "coast" window the track holds its last known position.
// ~1 s gives one missed second of coverage before deletion.
#define TRACKER_COAST_MS       1000UL

// Consecutive matched frames required before a track is shown to outputs.
// Kills single-frame ghost blips. At ~10 fps, 3 frames ≈ 300 ms lag to birth.
#define TRACKER_CONFIRM_FRAMES 3

// Exponential moving average weight for new measurements (position + speed).
// 0.0 = position never updates (frozen), 1.0 = no smoothing (raw jitter).
// 0.3 smooths well while still tracking a walking pace.
#define TRACKER_EMA_ALPHA      0.3f

// Speed (cm/s) below which a target counts as "not moving".
// The sensor reports small nonzero speeds even for a motionless target.
#define TRACKER_STILL_SPEED_CM 5.0f

// Time (ms) a target must stay below STILL_SPEED before we label it [STILL].
#define TRACKER_STILL_MS       10000UL

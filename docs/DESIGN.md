# Design Record — LD2450 Radar UI

> **Source of truth.** If code conflicts with this doc, fix the code. Ask before deviating from phase order.

---

## Phase map

| Phase | Scope | Status |
|-------|-------|--------|
| 0 | Bluetooth first light + ESP32 UART parser | **Done** |
| 1 | Tracker v1 — persistent IDs, smoothing, ghost-kill, coast, still-time | **Done** |
| 2 | On-device TFT radar UI driven by Tracker v1 output | **Active** |
| 2b | Wi-Fi debug mirror (Serial over UDP) | Parked — only if requested |
| 3 | Rear HC-SR04 integration, rear bubble fill | Not started |
| 4 | Audio / buzzer | Not started |
| — | BLE chest strap | Parked indefinitely — hardware not owned |

---

## Hardware

### ESP32

- Board: Elegoo ESP32 (classic ESP32-WROOM)
- Arduino IDE board: ESP32 Dev Module
- CPU: 240 MHz, Flash: 4 MB default partitions

### HLK-LD2450 radar

- Interface: Serial2, 256000 baud, 8N1
- GPIO 17 = Serial2 RX (sensor TX / white wire)
- GPIO 16 = Serial2 TX (sensor RX / red wire)
- Power: 5V → VIN, GND → GND

> **⚠️ NONSTANDARD CABLE COLORS — wire by PCB label, never by color**
> | PCB label | Cable color | ESP32 |
> |-----------|-------------|-------|
> | 5V  | Black  | VIN |
> | GND | Yellow | GND |
> | TX  | White  | GPIO 17 (Serial2 RX) |
> | RX  | Red    | GPIO 16 (Serial2 TX) |

### Inland 3.5" TFT (ILI9486 / may be ILI9488)

- Resolution: 480 × 320
- Driver: TFT_eSPI library
- SPI pins: SCK→18, MOSI→23, MISO→19, CS→15, DC→2, RST→4
- Power: 5V → VIN, GND → GND
- Touch (TP_CS→32, TP_IRQ→34) — optional, not used in Phase 2
- No backlight control pin on this header — dimming is palette-only

> **White screen on first power-up:** flip `ILI9486_DRIVER` to `ILI9488_DRIVER`
> in `User_Setup.h` (TFT_eSPI). No wiring change needed.

---

## Module layout (Phase 2)

```
config.h              — debug flags, tracker tuning, TFT palette constants
parser.h              — frame sync, sign-mag decode, rawTargets[3]
tracker.h             — Track struct, trackerInit/Update/Print
tft_ui.h              — all TFT_eSPI calls: radar bg, sweep, blips, debug overlay
ld2450-first-light.ino — setup() + loop(): parse → track → serialPrint → tftUpdate
```

Data flows one direction: parser fills `rawTargets[]`, tracker reads it and fills `tracks[]`, TFT UI reads `tracks[]` read-only. No module calls back up the chain.

---

## Tracker v1 output — what Phase 2 consumes

Phase 2 reads `tracks[3]` (global from `tracker.h`). Fields consumed by the UI:

| Field | Type | Meaning |
|-------|------|---------|
| `active` | bool | track is live (including coast window) |
| `confirmed` | bool | survived CONFIRM_FRAMES consecutive matches — **only confirmed tracks are drawn** |
| `id` | char | 'A', 'B', or 'C' — stable label for blip |
| `x` | float mm | smoothed horizontal position; positive = right |
| `y` | float mm | smoothed forward distance; positive = in front of sensor |
| `speed` | float cm/s | smoothed speed magnitude |
| `lastMoved` | uint32_t ms | millis() when speed last exceeded STILL_SPEED_CM |

Derived in UI (do not duplicate logic):
- `dist = sqrt(x² + y²)` in mm
- `angle = atan2(x, y)` in degrees (0° = straight ahead)
- `stillS = (millis() - lastMoved) / 1000.0`
- `isStill = (millis() - lastMoved) > TRACKER_STILL_MS` — use the config constant, do not hardcode

---

## TFT UI spec (Phase 2)

### Coordinate mapping

Sensor space → screen space:

- **Player marker** sits at `(SCREEN_W/2, PLAYER_Y)` where `PLAYER_Y ≈ 0.62 × SCREEN_H` (slightly below center).
- Forward zone maps from player marker upward (toward y=0 on screen).
- Scale: `~6 m = PLAYER_Y pixels`. At 480×320 with `PLAYER_Y≈200`, `1 m ≈ 33 px`.
- `screenX = SCREEN_W/2 + (track.x / RADAR_RANGE_MM) * PLAYER_Y`
- `screenY = PLAYER_Y  - (track.y / RADAR_RANGE_MM) * PLAYER_Y`

### Radar overlay

- **Range rings** at 1 m, 2 m, 4 m, 6 m — partial arcs clipped to forward zone.
- **Sweep line** rotates from the player marker at ~10 rpm (one full rotation per second). Driven by `millis()`, not blocking.
- **Forward wedge** ±60° (120° total) — full brightness.
- **Side wedges** ~105° each — dimmed, honest blind spot rendering. Never fake data here.
- **Rear bubble** below player marker — empty/dim in Phase 2; rear HC-SR04 is Phase 3.

### Blips

- One blip per confirmed track (`active && confirmed`), max 3.
- Blip position computed from `track.x`, `track.y` using the coordinate mapping above.
- Blip fades with age: brightness proportional to `(millis() - track.lastSeen)` against `TRACKER_COAST_MS`.
- Label: track `id` character (A/B/C).
- State label: `STILL` when `isStill`, otherwise `ACTIVE`. Labeled as motion inference — never claim sleep/HR/biometric.

### Debug overlay

- Per-blip: distance (mm or m) and angle (°), readable size on screen.
- Toggle with a `DEBUG_TFT` flag in `config.h` (same pattern as other debug flags).

### Palette / theme

All colors defined as named constants (e.g., `COL_BG`, `COL_RING`, `COL_SWEEP`, `COL_BLIP`, `COL_STILL`, `COL_DIM_ZONE`). Phase 2 uses green-on-black MW2 theme. Phase 3+ red night-vision theme is a constant swap, not a rewrite. No light-sensor hardware in Phase 2.

### Timing constraint

Screen update must NOT starve the radar parser. Strategy:
- `loop()` calls `syncHeader()` / `parseFrame()` / `trackerUpdate()` every iteration.
- TFT redraw happens on a `millis()` timer, max ~10 fps (100 ms interval).
- Only blips and sweep line redraw each frame; static background (rings, wedge outlines) redraws only on init or theme change.

---

## Out of scope

- Zone filtering (full field is the point)
- MQTT / Wi-Fi in core firmware
- Rear HC-SR04 (Phase 3)
- Audio / buzzer (Phase 4)
- BLE chest strap (parked)
- Housing / STL
- Second sensor
- Non-consenting sensing — this build only maps people in front of it in agreed demo use

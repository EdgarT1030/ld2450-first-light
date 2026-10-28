# LD2450 First Light — Phase 1: Tracker v1

ESP32 sketch that reads an HLK-LD2450 24 GHz radar and assigns persistent track IDs
(A / B / C) to up to three people, with EMA smoothing, ghost-rejection, and
per-track stillness detection.  Output goes to USB Serial Monitor (human-readable)
or CSV (for calibration logging).

---

## ⚠️ Wiring — nonstandard cable colors

> **This kit's cable colors are NOT the usual convention.**
> **Always wire by the labels printed on the sensor PCB, never by cable color.**

| Sensor PCB label | Cable color (this kit) | ESP32 pin        |
|------------------|------------------------|------------------|
| 5V               | Black                  | VIN              |
| GND              | Yellow                 | GND              |
| TX               | White                  | GPIO 17 (Serial2 RX) |
| RX               | Red                    | GPIO 16 (Serial2 TX) |

TX and RX cross over on purpose: sensor TX → ESP32 RX, sensor RX → ESP32 TX.

---

## Arduino IDE settings

| Setting           | Value                                        |
|-------------------|----------------------------------------------|
| Board             | ESP32 Dev Module                             |
| Upload Speed      | 921600 (lower to 460800 if uploads fail)     |
| CPU Frequency     | 240 MHz                                      |
| Flash Size        | 4MB (32Mb)                                   |
| Partition Scheme  | Default 4MB with spiffs                      |
| Port              | Whichever COM port appears after plugging in |

> **If upload stalls at "Connecting...":** Hold the **BOOT** button on the ESP32,
> click **Upload**, then release BOOT once you see `Writing at 0x...` in the output.

---

## Serial Monitor

Set baud rate to **115200**.

---

## Files

| File                      | Purpose                                            |
|---------------------------|----------------------------------------------------|
| `config.h`                | Debug flags and tuning constants — edit here first |
| `parser.h`                | Frame sync, sign-magnitude decode, `rawTargets[]`  |
| `tracker.h`               | Persistent track IDs, EMA smoothing, lifecycle     |
| `ld2450-first-light.ino`  | `setup()` and `loop()` only                        |

---

## Debug flags (config.h)

| Flag           | Effect                                                        |
|----------------|---------------------------------------------------------------|
| `DEBUG_SLOTS`  | Raw slot output only — restores Phase 0 behavior, no tracker |
| `DEBUG_RAW`    | Hex-dumps every 30-byte frame for protocol debugging         |
| `DEBUG_CSV`    | Emits CSV lines instead of human-readable output             |

Set exactly one of these to `1` at a time; leave the others `0`.

---

## Tracker v1 behavior

- **Persistent IDs** — each detected person keeps the same letter (A/B/C) across
  frames, regardless of which raw sensor slot they appear in.
- **Birth gate** — a blip must appear in 3 consecutive frames before it is shown.
  Single-frame ghosts (reflections, hand-waves) are silently dropped.
- **EMA smoothing** — position and speed are smoothed with a configurable
  exponential moving average (`TRACKER_EMA_ALPHA = 0.3` by default).
- **Coast window** — a track that loses its measurement coasts for 1 second at its
  last known position before being deleted.  This handles brief occlusions.
- **Stillness label** — once a track's smoothed speed stays below 5 cm/s for
  10 seconds, it is labelled `[STILL]`.

All thresholds are in `config.h`.

---

## Human-readable Serial output

```
[TRACK] A  x= 320mm  y=1450mm  dist=1485mm  angle= 12.4deg  spd= 8.2cm/s  still= 0.0s  [MOVING]
[TRACK] B  x=-150mm  y=2100mm  dist=2105mm  angle= -4.1deg  spd= 0.3cm/s  still=45.2s  [STILL]
---
```

Fields: smoothed X/Y (mm), distance from sensor (mm), horizontal angle (0° = straight
ahead, positive = right, negative = left), smoothed speed (cm/s), time since last
movement above 5 cm/s.

---

## Calibration / CSV mode

Set `DEBUG_CSV 1` in `config.h`.  Open **Tools → Serial Monitor**, check **Autoscroll**,
copy-paste output into a spreadsheet.  Or use a terminal with logging:

```
CSV,millis,id,x_mm,y_mm,dist_mm,angle_deg,speed_cms,still_s
CSV,5023,A,318,1448,1483,12.4,8.2,0.0
CSV,5123,A,315,1445,1479,12.3,6.9,0.1
```

---

## Bench test protocol

Run each test in order.  All tests use Serial Monitor at 115200 with `DEBUG_SLOTS 0`,
`DEBUG_CSV 0`, `DEBUG_RAW 0` (default `config.h`).

**Setup:** sensor mounted vertically at chest height (~1.2 m), antenna facing the
open room, nothing metal directly behind the sensor.

---

### Test 1 — Solo walk (track ID stability)

Walk slowly in front of the sensor from left to right at ~2 m, then back.

**Pass:** A single track (`[TRACK] A`) appears and holds its ID throughout.
No `[TRACK] B` born.  Position moves smoothly; no teleporting jumps.

**Fail:** ID changes mid-walk, or a second track appears while only one person
is present.  If IDs swap, lower `TRACKER_EMA_ALPHA` or raise `TRACKER_MAX_JUMP_MM`
in `config.h`.

---

### Test 2 — Stand still (stillness detection)

Walk to ~2 m from the sensor, stop completely, wait 15 seconds.

**Pass:** Track appears, speed drops to < 5 cm/s within a few seconds, and
`[STILL]` label appears within ~10 s of stopping.  The `still=` counter keeps
increasing.

**Fail:** Track keeps flipping between `[MOVING]` and `[STILL]` while you are
stationary.  Raise `TRACKER_STILL_SPEED_CM` slightly in `config.h`.

---

### Test 3 — Ghost rejection

Stand at ~2 m, hold one hand close to the sensor (~30 cm), wave it once, pull it back.
Remain still.

**Pass:** Your main track (A) stays confirmed.  No second track (B) is born and
confirmed from the brief hand detection.

**Fail:** `[TRACK] B` appears and persists.  Increase `TRACKER_CONFIRM_FRAMES`
in `config.h`.

---

### Test 4 — Two people, side by side

Two people stand ~1 m apart, both at ~2 m range, facing the sensor.

**Pass:** Two confirmed tracks (A and B) appear.  IDs remain stable as each person
shifts weight or takes a small step.

**Fail:** Only one track visible despite two people present.  The sensor may be
resolving them as one target at this angle/distance — step further apart or move
to 3–4 m range.

**Note in your calibration log:** minimum separation at which two people are
reliably seen as two tracks at 2 m / 3 m / 4 m / 6 m.  Use `DEBUG_CSV 1` to
capture the data.

---

### Test 5 — Two people, one behind the other (occlusion)

Two people line up on the same angle, front person ~2 m, rear person ~3 m.

**Expected:** sensor may report only one target while they are exactly aligned.
Both tracks should coast through the overlap.

**Pass:** When the rear person steps ~0.5 m to one side, both tracks reappear
(possibly with short born-delay from CONFIRM_FRAMES).  No spurious third track born.

**Fail:** A ghost third track is born during alignment.  Reduce `TRACKER_EMA_ALPHA`
or raise `TRACKER_MAX_JUMP_MM` so coasting tracks are not prematurely deleted.

---

### Test 6 — Three people

Three people stand at different positions within ~4 m.

**Pass:** Three confirmed tracks (A, B, C) appear and remain stable.

**Note:** this is the sensor's hardware maximum.  A fourth person will not appear
even if present — that is expected behavior, not a bug.

---

### Test 7 — Target leaves and returns

One person walks out of sensor range (> 6 m or behind a wall) and returns.

**Pass:** Track is deleted within ~1 s of leaving range.  A new track is born on
return (may receive a different letter — that is correct behavior for v1).

---

## Tuning guide (config.h)

| Symptom                              | Adjust                                        |
|--------------------------------------|-----------------------------------------------|
| ID swaps during quick movement       | Raise `TRACKER_MAX_JUMP_MM`                   |
| Ghost tracks from brief detections   | Raise `TRACKER_CONFIRM_FRAMES`                |
| Position jitters when standing still | Lower `TRACKER_EMA_ALPHA`                     |
| Track slow to follow fast movement   | Raise `TRACKER_EMA_ALPHA`                     |
| STILL label appears too soon         | Raise `TRACKER_STILL_MS`                      |
| Track deleted during brief occlusion | Raise `TRACKER_COAST_MS`                      |

---

## Protocol quick-reference

| Field           | Value                                                        |
|-----------------|--------------------------------------------------------------|
| Baud rate       | 256000 (8N1) — 9600 will not work                            |
| Frame length    | 30 bytes                                                     |
| Header          | `AA FF 03 00`                                                |
| Tail            | `55 CC`                                                      |
| Target slots    | 3 slots × 8 bytes, little-endian                             |
| Slot layout     | X (2 B), Y (2 B), speed (2 B), resolution (2 B)             |
| Encoding        | X / Y / speed are **sign-magnitude** — bit 15 is the sign   |
| Units           | X/Y in mm, speed in cm/s, resolution in mm                   |
| Empty slot      | resolution == 0                                              |
| Coordinate axes | X positive = right of sensor, Y positive = in front          |
| Speed sign      | Positive = moving away from sensor                           |
| Max range       | ~6 m (~20 ft)                                                |
| Zone filtering  | OFF — full field reported                                    |

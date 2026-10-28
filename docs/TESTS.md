# Test Protocol

> Run in order. All tests assume default `config.h` (all debug flags 0) unless noted.
> Serial Monitor at 115200 baud stays open alongside the screen throughout Phase 2.

---

## Phase 1 bench acceptance (already passed)

### T1-1 Solo walk — track ID stability
Walk slowly left-to-right at ~2 m, then back.
**Pass:** Single track A holds its ID throughout. No ghost B born. Position moves smoothly.

### T1-2 Stand still — stillness detection
Walk to ~2 m, stop, wait 15 s.
**Pass:** `[STILL]` label appears within ~10 s of stopping. Clears when moving again.

### T1-3 Ghost rejection
Wave hand close (~30 cm) once; remain still.
**Pass:** Main track stays confirmed. No second track born and confirmed from the wave.

### T1-4 Two people side by side
Two people ~1 m apart at ~2 m range.
**Pass:** Two confirmed tracks A and B, stable IDs.

### T1-5 Two people in a line (occlusion)
Front ~2 m, rear ~3 m, same angle.
**Expected:** One target visible during alignment. Both reappear when rear person steps aside.
**Pass:** No spurious third track born during alignment.

### T1-6 Three people
Three people within ~4 m.
**Pass:** Three confirmed tracks A/B/C, stable.

### T1-7 Target leaves and returns
One person walks out of range and returns.
**Pass:** Track deleted within ~1 s of leaving. New track born on return (ID may differ — correct for v1).

---

## Phase 2 bench acceptance

Run these after Phase 2 firmware is uploaded. Serial Monitor stays open to compare screen blips against serial track output.

### T2-1 Screen comes up — no white screen
Power on.
**Pass:** Radar background, rings, and sweep line appear within 3 s.
**Fail/fix:** If screen is white, flip `ILI9486_DRIVER` → `ILI9488_DRIVER` in TFT_eSPI `User_Setup.h`. No wiring change.

### T2-2 Solo walk — blip tracks with consistent ID
Walk slowly in front of sensor at ~2 m.
**Pass:** One blip follows you on screen, holds its ID letter (A), matches the `[TRACK] A` line on Serial Monitor. No ghost blips.

### T2-3 Stand still — STILL label on screen
Walk to ~2 m, stop, wait 15+ s.
**Pass:** `STILL` label appears on the blip within ~10 s (same threshold as Tracker v1). Clears when you move again. Serial Monitor `[STILL]` and screen label agree.

### T2-4 Two people side-by-side
Two people ~1 m apart at ~2 m range.
**Pass:** Screen shows exactly what the tracker reports — two blips if tracker has two confirmed tracks, one if it merged them. No invented second blip.

### T2-5 Two people in a line
Front ~2 m, rear ~3 m, same angle.
**Pass:** Screen matches Serial Monitor. No hallucinated second blip during occlusion merge. Both reappear when they separate.

### T2-6 Debug overlay readable
Enable `DEBUG_TFT 1` in `config.h`, reupload.
**Pass:** Distance (mm or m) and angle (°) visible next to each blip at a readable font size.

### T2-7 Sweep does not starve parser
Walk briskly across the field at ~2 m.
**Pass:** Blip keeps up with movement. No Serial Monitor errors (`[ERR] frame body timeout`). No visible lag >~2 frames.

### T2-8 Rear bubble is empty
No target behind the sensor.
**Pass:** Rear bubble zone is dim/empty. No blips or labels drawn there. (Rear HC-SR04 is Phase 3.)

---

## Notes

- Blip state labels (`STILL` / `ACTIVE`) are inferences from motion data only. Never claim they measure sleep, heart rate, or any biometric.
- If T2-3 threshold feels wrong on bench, adjust `TRACKER_STILL_MS` in `config.h` — do not change the UI label logic separately.

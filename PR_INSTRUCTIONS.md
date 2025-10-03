
# PR-Ready: Build 6.2.3 (Spot Pattern – Stable Polish)

This PR adds:
- `src/feature_623.cpp` → routes & handlers for `/version.json` and calibration endpoints
- `.github/workflows/ci_ota.yml` → OTA artifacts always available
- Bumps `BUILD_VERSION` → `Build_6_2_3_SpotPattern`

## Minimal code change to your existing main.cpp
1) Add this line **inside `setup()`**, after you create the WebServer and before/after other `server.on(...)` routes:
```cpp
extern "C" void initRoutes623();
initRoutes623();
```
> This is the ONLY line you need to add. No other edits required.

2) Build locally or run the workflow `CI OTA Artifacts (6.2.3)`.

## Endpoints
- `GET  /version.json` → `{ build, op_mode, pattern, gap_ms }`
- `POST /api/calib/zero_v` → `{ v_off }`  (average ADC on ZMPT)
- `POST /api/calib/zero_i` → `{ i_off }`  (average ADC on ACS712)
- `POST /api/calib/scale?v=...&i=...` → `{ v_sc, i_sc }`

## OTA artifacts
- `spotweldingplus-app-Build_6_2_3.bin` → upload to `/update`
- `spotweldingplus-merged-Build_6_2_3.bin` → flash @ 0x0 (optional)

## Notes
- No forced `-include` flags. No long string literals.
- Uses existing globals: `server`, `prefs`, `opMode`, `spotPattern`, `gapMs`, `V_offset`, `I_offset`, `V_scale`, `I_scale`.
- ZMPT/ACS712 pins must be defined in your build; otherwise endpoints reply 400.

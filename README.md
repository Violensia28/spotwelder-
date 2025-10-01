# Clean Build Starter (Web UI only, AP-only) — Build #1 Green

Use this starter to create a **clean repo** that compiles on the first try using GitHub Actions.

## Steps
1. Create a **new GitHub repo** (empty) and upload this ZIP.
2. Go to **Actions** → Run **Build Firmware**.
3. Download artifact **firmware.bin**.
4. Power ESP32 → connect to AP **SPOTWELD+** (pass `weld12345`) → open `http://192.168.4.1/`.

## What’s inside
- `platformio.ini` pinned (`espressif32@6.6.0`) and **restricted `src_filter`** (only builds `main.cpp`, `ui_assets_min.cpp`, `patch_ssr_logger_stub.cpp`).
- Minimal UI assets in code (`ui_assets_min.*`) to satisfy linker (no external FS).
- `SSRLog` stub for CSV to avoid logger compile errors.
- GitHub Actions workflow using `python -m platformio` (no pipx path issues).

## Next
Once **Build #1** is green, we can:
- Replace `ui_assets_min.*` with the full modular Web UI assets.
- Replace `SSRLog` stub with the full implementation.
- Enable OTA upload and optional WebSockets.

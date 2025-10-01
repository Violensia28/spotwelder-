# spotwelding+ (Build 6.1 — Smart Page terpisah `/smart`)

Pemisahan UI Smart AI Spot ke halaman khusus `/smart` agar **tidak bentrok** dengan UI Preset 99.

## Yang baru
- Halaman **Home** (`/`): Trigger, Live Sensor, link ke **/smart**, OTA, Guard status ringkas.
- Halaman **Smart** (`/smart`): Smart AI Spot lengkap (thickness Ni, baseline & tuned, Start/Stop/Apply, status RUNNING/IDLE, rating, metrik terakhir).
- Alias: `/smart/` → redirect ke `/smart` (anti salah ketik).
- **API tetap sama** (`/smart/*`, `/api/*`).
- **BUILD_VERSION**: `Build_6_1_SmartPage` (escape quotes di `platformio.ini` sudah benar).

## Build & Update
- CI: *Actions → CI Build* → artifacts (`spotweldingplus-app.bin`, `spotweldingplus-merged.bin`, `.elf`).
- Flash awal: `merged.bin @ 0x0` (Android Flasher).
- Update berikutnya: **OTA** upload `app.bin` ke `/update`.

## Catatan
- Guard & Auto‑Trigger tetap aktif di kedua halaman.
- Smart AI hanya mengubah durasi saat **RUNNING** atau setelah **Apply** (menyimpan tuned di NVS per ketebalan).

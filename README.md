# spotwelding+ (Build #3 — Preset 99 + Filter)

Build #3 menambahkan **UI Preset 99** dengan **Filter Grup** + pemilihan preset yang disimpan ke NVS.

## Fitur
- **UI Preset 99** dengan filter grup (Ni‑Thin, Ni‑Med, Ni‑Thick, Al, Cu, Custom)
- **Pemilihan preset** tersimpan (NVS) dan dipakai saat **Trigger**
- **Non‑blocking FSM** untuk pre‑pulse → gap → main‑pulse
- **SoftAP only** (SSID `SpotWelder_AP`, Pass `12345678`)
- **OTA Update** tetap via `/update`

## Endpoint & UI
- Web UI: `http://192.168.4.1/` atau `/ui`
- OTA: `http://192.168.4.1/update` (default `admin/admin`)
- API: `/api/presets`, `/api/current`, `/api/select?id=NUM`
- Versi: `/version` → `Build_3_Presets`

## Catatan Preset
Preset 1..99 dibuat otomatis berdasarkan level → durasi *(pre, main)* secara terukur:
- Pre‑pulse: 0..80 ms (naik bertahap)
- Main pulse: ~70..300 ms (linear bertahap)
- Gap tetap: 60 ms (sementara)

> Grup digunakan untuk **filter tampilan**; mapping level → grup: 1–20 Ni‑Thin, 21–40 Ni‑Med, 41–60 Ni‑Thick, 61–78 Al, 79–96 Cu, 97–99 Custom.

## Build & Flash
- PlatformIO upload biasa (atau flash awal via `spotweldingplus-merged.bin @ 0x0`)
- Update berikutnya: **OTA** unggah `spotweldingplus-app.bin`

## Keamanan
Ganti kredensial OTA di `include/Config.h` sebelum rilis lapangan.

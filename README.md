# spotwelding+ (Build 6.2 — Mode Switch & History)

Build 6.2 menambahkan **Operation Mode** (Preset ↔ Smart) tersimpan di **NVS** dan **History Auto‑Tune** (JSON + **CSV download**).

## Fitur Baru
- **Mode Switch** (`/api/mode` + selector di Home):
  - `PRESET` → gunakan **Preset 1..99** seperti biasa.
  - `SMART` → gunakan **tuned pre/main** dari Smart AI (per ketebalan Ni).
- **History Auto‑Tune**:
  - Endpoint: `GET /smart/history` (JSON) & `GET /smart/history.csv` (CSV)
  - Kolom: `ms, t_mm, pre_ms, main_ms, E_est, irms, rating`
  - Ring buffer 32 entri (latest first).
- **Halaman**: `/` (Home) dengan **Mode selector** & link ke `/smart`; `/smart` berisi Smart AI penuh + tabel History.

## Endpoint Baru
```http
GET  /api/mode                     # → {"mode":"PRESET"|"SMART"}
POST /api/mode?m=PRESET|SMART      # set & simpan ke NVS
GET  /smart/history                # JSON ringkas (latest first)
GET  /smart/history.csv            # CSV download
```

## Catatan Integrasi
- **Guard** & **Auto‑Trigger** tetap aktif di kedua mode.
- **Apply Tuned** tidak otomatis mengubah mode; memakai hasil tuned **langsung** hanya jika mode = SMART.
- **Tebal Ni aktif** disimpan di NVS (`smart_t`).

## Build & Update
- Flash awal: `spotweldingplus-merged.bin @ 0x0` (Android Flasher)
- Update berikutnya: **OTA** → unggah `spotweldingplus-app.bin` ke `/update`

## Versi
- `/version` → `Build_6_2_ModeHistory`

# spotwelding+ (Build 7 — Detail Logging)

**Build 7** menambahkan **pencatatan detail per siklus weld** (RAM ring buffer, JSON/CSV download) tanpa memerlukan filesystem.

## Fitur Baru
- **Detail Weld Log** (ring buffer `LOG_MAX=256`):
  - `ts_ms`, `result` (DONE / ABORT_OC / ABORT_UV)
  - `mode` (PRESET/SMART), `pattern` (SINGLE/DOUBLE)
  - `preset_id`, `t_mm` (SMART)
  - `pre_ms`, `gap_ms`, `main_ms`
  - **Baseline**: `vrms_idle`, `irms_idle`
  - **Pre stats**: `pre_irms_avg`, `pre_irms_max`, `pre_vrms_min`
  - **Main stats**: `main_irms_avg`, `main_irms_max`, `main_vrms_min`
  - **Guard** thresholds: `i_trig`, `v_cut`, `i_lim`
  - **AI** `rating` (jika ada)
- **API Log**:
  - `GET /log/summary` → JSON latest first
  - `GET /log.csv` → unduh CSV
  - `POST /log/clear` → kosongkan log
- **UI**:
  - **Home**: kartu **Weld Logs** (tabel ringkas, tombol **Clear**, link **Download CSV**)
  - **/smart**: link **Download Weld Logs CSV** + tabel ringkas

## Cara Kerja (ringkas)
- Saat **START**: simpan `vrms_idle` & `irms_idle`, reset akumulator fase.
- Saat **PRE/MAIN**: akumulasi `Irms` rata-rata & puncak, `Vrms` minimum (indikasi sag). 
- Saat **ABORT** (OC/UV) atau **END**: buat entri **WeldLogEntry** → push ke ring buffer.

## Endpoint Terkait Lain (dari Build 6.2.2)
- Mode operasi: `GET/POST /api/mode` (PRESET ↔ SMART)
- Spot Pattern & Gap: `GET/POST /api/weldcfg`
- Smart AI: `/smart/*` (options, config, start/stop/apply, status, history & CSV)
- Guard: `/api/guard/*` (load/save/status)
- Sensor: `/api/sensor`
- OTA: `/update`

## Build & Update
- OTA: unggah `spotweldingplus-app.bin` ke `/update`.
- Fresh/Recovery: flash `spotweldingplus-merged.bin @ 0x0`.
- Cek versi: `/version` → `Build_7_DetailLogging`.

# spotwelding+ (Build 6.2.2 — Spot Pattern + Gap)

Build 6.2.2 menambahkan **Spot Pattern selector** (Single / Double) dan **Gap editor** (persist **NVS**) yang berlaku di **keduanya**: Mode **PRESET** maupun **SMART**.

## Fitur Baru
- **Spot Pattern** (`/api/weldcfg`)
  - `SINGLE` → memaksa `pre_ms = 0` (efektif single pulse)
  - `DOUBLE` → gunakan `pre_ms` dari PRESET/SMART (double pulse)
- **Gap (ms)** disimpan di NVS (`w_gap`) dan dipakai saat `pre_ms > 0`.
- **UI**: Home **dan** `/smart` punya panel **Spot Pattern + Gap**.

## Endpoint Baru
```http
GET  /api/weldcfg                 # → {"pattern":"SINGLE|DOUBLE","gap_ms":60}
POST /api/weldcfg?pattern=DOUBLE&gap_ms=60
```

## Integrasi
- **Operation Mode** tetap ada (`/api/mode` PRESET ↔ SMART)
- **Smart AI** tetap bekerja; hasil tuned dipakai penuh saat `pattern=DOUBLE`, atau pre dipaksa 0 saat `pattern=SINGLE`.
- **Guard** & **Auto‑Trigger** tetap aktif.

## Build & Update
- **OTA**: upload `spotweldingplus-app.bin` ke `/update`
- **Fresh/Recovery**: `spotweldingplus-merged.bin @ 0x0`

## Versi
- `/version` → `Build_6_2_2_SpotPattern`

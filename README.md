# spotwelding+ (Build #5 — Auto‑Trigger + Guard)

> **Disclaimer**: Fitur ini tidak menggantikan **proteksi hardware** (MCB, fuse, MOV/snubber, SSR rating). Pastikan SOP keselamatan dipatuhi.

## Fitur Baru
- **Auto‑Trigger**: siklus las otomatis saat **Irms ≥ I_trig** dan **Vrms ≥ V_cut** stabil beberapa window.
- **V/I Guard**: tahan saat **Vrms** di bawah **V_cut**, **abort** jika **Irms** melebihi **I_limit** selama siklus.
- **Cooldown** antar siklus untuk menurunkan inrush/thermal stress.
- **Event Log** ring buffer (`/api/log`).

## Default (bisa diubah di UI)
- Auto‑Trigger: **ON**
- Guard: **ON**
- I_trig = **2.0 A**
- V_cut = **150 V**
- I_limit = **35 A**
- Cooldown = **1500 ms**
- Stable windows = **2** (tiap window ≈ `SENSE_WIN_MS` = 250 ms)

## Endpoint Baru
- `/api/guard/load` → konfigurasi guard
- `/api/guard/save?auto=0|1&guard=0|1&i_trig=..&v_cut=..&i_lim=..&cd=..&sw=..`
- `/api/guard/status` → status (ready/hold, cooldown, last event)
- `/api/log` → array string event log

## Cara Pakai
1. **Pastikan Build #4 kalibrasi sudah benar** (Zero & Scale) agar threshold akurat.
2. Atur **I_trig**, **V_cut**, **I_limit**, **Cooldown**, **Stable windows** di panel **Auto‑Trigger & Guard**.
3. Aktifkan **Auto‑Trigger** dan **Guard** (default ON) → sistem akan men‑trigger saat arus & tegangan terpenuhi.

## Catatan Teknis
- Keputusan auto‑trigger berbasis **RMS window** (default 250 ms). Untuk respons lebih cepat, kurangi `SENSE_WIN_MS` (dengan trade‑off noise).
- **Abort** saat weld: jika `Irms > I_limit` atau `Vrms < V_cut`.
- **Cooldown** menahan retrigger agar MCB tidak sering trip.

## Build & Update
- Flash awal: `spotweldingplus-merged.bin @ 0x0` (Android Flasher)
- Update berikutnya: **OTA** unggah `spotweldingplus-app.bin` ke `/update`

## Keamanan
- Ganti kredensial OTA di `include/Config.h`.
- Fitur Guard bersifat **software‑level**; tetap gunakan proteksi hardware.

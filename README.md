# spotwelding+ (Build #4 — Sensor + Kalibrasi)

Build #4 menambahkan **halaman Sensor (Vrms, Irms)** dan **Kalibrasi (offset & scale)** untuk ACS712 (arus) dan ZMPT (tegangan), disimpan ke NVS.

## Fitur Utama
- **Live Sensor**: Vrms & Irms (window ~250 ms)
- **Kalibrasi**:
  - **Zero Current / Zero Voltage** (set offset dari kondisi tanpa beban/AC)
  - **Scale** (A/count, V/count) manual atau **Auto‑Scale** berdasarkan target (mis. 220V)
  - Disimpan ke **NVS** (persisten)
- **UI Preset 99 + Filter** tetap ada; **OTA** via `/update`
- **Non‑blocking loop** (sampling ringan per loop)

## Endpoint
- `/api/sensor` → `{ vrms, irms, n, win_ms }`
- `/api/calib/load` → `{ i_off, v_off, i_sc, v_sc }`
- `/api/calib/zero?ch=i|v` → set offset (ambil 256 sampel)
- `/api/calib/save?i_sc=..&v_sc=..` → simpan skala
- `/api/calib/auto_v?target=220` → V_scale dari sinyal saat ini
- `/api/calib/auto_i?target=10` → I_scale dari sinyal saat ini

## Prosedur Kalibrasi (Disarankan)
1) **Zero Voltage**: lepas sumber AC dari ZMPT → klik *Zero Voltage*  
2) **Zero Current**: tanpa arus pada ACS712 → klik *Zero Current*  
3) **Auto‑Scale Voltage**: sambungkan AC referensi (mis. 220V) → *Auto‑Scale Voltage (to target)*  
4) **Auto‑Scale Current**: alirkan arus referensi (mis. clamp meter = 10A) → *Auto‑Scale Current*  
5) **Save** → nilai tersimpan ke NVS

> **Catatan:** Nilai default `I_scale`/`V_scale` adalah placeholder. Lakukan kalibrasi agar akurat. Performa ADC ESP32 dipengaruhi noise/attenuasi/kabel; gunakan PSU & wiring yang baik.

## Build & Update
- Flash awal: `spotweldingplus-merged.bin @ 0x0` (Android Flasher)
- Update berikutnya: **OTA** → unggah `spotweldingplus-app.bin` ke `/update`

## Keamanan
- Ganti kredensial OTA di `include/Config.h`.

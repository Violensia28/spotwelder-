# spotwelding+ (Build #2 — OTA)

Build #2 menambahkan **OTA (Over-The-Air) Web Update** via halaman `/update`.

## Fitur
- **SoftAP only** (SSID `SpotWelder_AP`, Pass `12345678`)
- **Web UI**: tombol Trigger + Web Audio beep
- **OTA**: upload file `.bin` langsung dari browser ke `http://192.168.4.1/update`
  - Basic Auth: `admin` / `admin` (ubah di `include/Config.h`)
- **Pins**: SSR=GPIO26, ACS712=GPIO34, ZMPT=GPIO35

## Cara OTA
1. Hubungkan HP/PC ke Wi‑Fi `SpotWelder_AP`
2. Buka `http://192.168.4.1/update`
3. Login (default `admin`/`admin`)
4. Pilih file **spotweldingplus-app.bin** (artifact dari CI/Release)
5. Klik **Upload** → device akan reboot otomatis

## Build Lokal
- PlatformIO: `Upload` seperti biasa
- Serial monitor: 115200 baud

## CI & Release
- **CI Build** (manual): menghasilkan artifact
  - `spotweldingplus-app.bin` ← gunakan untuk OTA
  - `bootloader.bin`, `partitions.bin`, `spotweldingplus-merged.bin`, `.elf`
- **Release** (push tag `v*`): publish semua file otomatis

## Keamanan
Ganti kredensial OTA di `include/Config.h`:
```c
#define OTA_USER "admin"
#define OTA_PASS "admin"
```

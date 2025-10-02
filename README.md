# spotwelding+ (Build 6.2.1 — Mode Switch & History, Fix)

Hotfix untuk error compile **'handleGuardStatus was not declared'**:
- Tambah **forward declarations** untuk semua handler API sebelum `setup()`.
- `BUILD_VERSION` → `Build_6_2_1_ModeHistoryFix` (quotes sudah di‑escape di `platformio.ini`).

Cara update: OTA upload `spotweldingplus-app.bin` ke `/update`, atau flash `merged.bin @ 0x0`.

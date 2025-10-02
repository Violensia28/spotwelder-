# spotwelding+ (Build 7.0.1 — Detail Logging Fix)

Hotfix untuk error linker **undefined reference to `smartHistoryJson` / `smartHistoryCsv`** pada Build 7.

## Perbaikan
- Mengembalikan implementasi endpoint **/smart/history** (JSON) dan **/smart/history.csv** (CSV) yang terlewat saat refactor Build 7.
- `BUILD_VERSION` → `Build_7_0_1_DetailLoggingFix`.

## Cara update
- Ganti/patch fungsi yang hilang, atau gunakan paket ini lalu **OTA** seperti biasa.

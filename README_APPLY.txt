
HOW TO APPLY (GitHub Web):
1) In your repo, click: Add file → Upload files.
2) Upload these files preserving their paths:
   - include/hotfix_forward_decls.h
   - src/hotfix_smart_history_impl.cpp
   - platformio.ini  (replace if asked)
3) Commit. Then build your CI or local PIO.

Notes:
- We do NOT touch your main.cpp. The header is force-included via build_flags.
- Endpoints return empty JSON/CSV (safe). Your other features remain unchanged.
- Version is set to Build_7_0_1_DetailLoggingFix for traceability.

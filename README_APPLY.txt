
HOW TO APPLY (GitHub Web):
1) In your repo, click: Add file → Upload files.
2) Upload these files preserving their paths:
   - include/hotfix_forward_decls.h
   - src/hotfix_smart_history_impl.cpp
   - platformio.ini  (replace if asked)
3) Commit → run CI or local PlatformIO build.

Notes:
- No edits to your main.cpp needed.
- Endpoints return empty JSON and CSV header only (safe placeholder).
- We use a char array (not a quoted string) to avoid line-break issues when copying.
- Version set to Build_7_0_1_DetailLoggingFix for traceability.

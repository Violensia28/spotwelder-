#ifndef CONFIG_H
#define CONFIG_H
#define SSR_PIN 26
#define ACS712_PIN 34
#define ZMPT_PIN 35
#define BUZZER_PIN 27
#define PRESET_COUNT 99
#ifndef BUILD_VERSION
#define BUILD_VERSION "Build_7_0_1_DetailLoggingFix"
#endif
#define AP_SSID "SpotWelder_AP"
#define AP_PASS "12345678"
#define OTA_USER "admin"
#define OTA_PASS "admin"
#define DEF_I_OFFSET 2048
#define DEF_V_OFFSET 2048
#define DEF_I_SCALE  0.00050f
#define DEF_V_SCALE  0.10000f
#define SENSE_WIN_MS 250
#define DEF_AUTO_EN      1
#define DEF_GUARD_EN     1
#define DEF_I_TRIG_A     2.0f
#define DEF_V_CUTOFF_V   150.0f
#define DEF_I_LIMIT_A    35.0f
#define DEF_COOLDOWN_MS  1500
#define DEF_STABLE_WIN   2
#define AI_MAX_TRIALS    4
#define AI_UP_MAIN_PCT   0.15f
#define AI_UP_PRE_PCT    0.08f
#define AI_DOWN_MAIN_PCT 0.15f
#define AI_FINE_PCT      0.05f
#define AI_MIN_PRE_MS    10
#define AI_MAX_PRE_MS    80
#define AI_MIN_MAIN_MS   60
#define AI_MAX_MAIN_MS   300
#define AI_BAND_LOW      0.90f
#define AI_BAND_HIGH     1.10f
#define MODE_PRESET 0
#define MODE_SMART  1
#define DEF_OP_MODE MODE_PRESET
#define AI_HIST_MAX 32
#define PATTERN_SINGLE 0
#define PATTERN_DOUBLE 1
#define DEF_SPOT_PATTERN PATTERN_DOUBLE
#define DEF_GAP_MS 60
#define LOG_MAX 256
#endif

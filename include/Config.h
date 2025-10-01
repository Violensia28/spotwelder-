#ifndef CONFIG_H
#define CONFIG_H

// ==== Pins ====
#define SSR_PIN 26
#define ACS712_PIN 34
#define ZMPT_PIN 35
#define BUZZER_PIN 27 // Dummy (Web Audio beep used)

// ==== Presets ====
#define PRESET_COUNT 99

#ifndef BUILD_VERSION
#define BUILD_VERSION "Build_6_1_SmartPage"
#endif

// ==== Wi-Fi / OTA ====
#define AP_SSID "SpotWelder_AP"
#define AP_PASS "12345678"
#define OTA_USER "admin"
#define OTA_PASS "admin"

// ==== Sensor Defaults (tune by calibration) ====
#define DEF_I_OFFSET 2048
#define DEF_V_OFFSET 2048
#define DEF_I_SCALE  0.00050f   // A per ADC count (placeholder)
#define DEF_V_SCALE  0.10000f   // V per ADC count (placeholder)

// ==== Sensor window (ms) ====
#define SENSE_WIN_MS 250

// ==== Auto-Trigger & Guards (defaults) ====
#define DEF_AUTO_EN      1
#define DEF_GUARD_EN     1
#define DEF_I_TRIG_A     2.0f
#define DEF_V_CUTOFF_V   150.0f
#define DEF_I_LIMIT_A    35.0f
#define DEF_COOLDOWN_MS  1500
#define DEF_STABLE_WIN   2

// ==== Smart AI (bounds & steps) ====
#define AI_MAX_TRIALS    4
#define AI_UP_MAIN_PCT   0.15f   // increase main when cold (15%)
#define AI_UP_PRE_PCT    0.08f   // increase pre when cold (8%)
#define AI_DOWN_MAIN_PCT 0.15f   // decrease main when hot (15%)
#define AI_FINE_PCT      0.05f   // fine tune when OK (5%)
#define AI_MIN_PRE_MS    10
#define AI_MAX_PRE_MS    80
#define AI_MIN_MAIN_MS   60
#define AI_MAX_MAIN_MS   300
#define AI_BAND_LOW      0.90f   // 90% of base energy
#define AI_BAND_HIGH     1.10f   // 110% of base energy

#endif

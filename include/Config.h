#ifndef CONFIG_H
#define CONFIG_H

#define SSR_PIN 26
#define ACS712_PIN 34
#define ZMPT_PIN 35
#define BUZZER_PIN 27 // Dummy (Web Audio beep used)

#define PRESET_COUNT 99

#ifndef BUILD_VERSION
#define BUILD_VERSION "Build_5_AutoGuard"
#endif

// SoftAP credentials
#define AP_SSID "SpotWelder_AP"
#define AP_PASS "12345678"

// OTA auth (basic HTTP auth)
#define OTA_USER "admin"
#define OTA_PASS "admin"

// Sensor defaults (can be calibrated)
#define DEF_I_OFFSET 2048
#define DEF_V_OFFSET 2048
#define DEF_I_SCALE  0.00050f   // A per ADC count (placeholder)
#define DEF_V_SCALE  0.10000f   // V per ADC count (placeholder)

// Sensor window (ms)
#define SENSE_WIN_MS 250

// Auto-Trigger & Guards (defaults)
#define DEF_AUTO_EN      1   // 1=enable auto-trigger
#define DEF_GUARD_EN     1   // 1=enable V/I guards
#define DEF_I_TRIG_A     2.0f   // trigger when Irms >= this (A)
#define DEF_V_CUTOFF_V   150.0f // permit weld only if Vrms >= this (V)
#define DEF_I_LIMIT_A    35.0f  // abort weld if Irms > this (A)
#define DEF_COOLDOWN_MS  1500   // min time between weld cycles
#define DEF_STABLE_WIN   2      // windows (SENSE_WIN_MS) required to arm trigger

#endif

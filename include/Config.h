#ifndef CONFIG_H
#define CONFIG_H

#define SSR_PIN 26
#define ACS712_PIN 34
#define ZMPT_PIN 35
#define BUZZER_PIN 27 // Dummy (Web Audio beep used)

#define PRESET_COUNT 99

#ifndef BUILD_VERSION
#define BUILD_VERSION "Build_4_Sensor"
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
#define DEF_I_SCALE  0.00050f   // A per ADC count (placeholder, to be calibrated)
#define DEF_V_SCALE  0.10000f   // V per ADC count (placeholder, to be calibrated)

// Sensor window (ms)
#define SENSE_WIN_MS 250

#endif

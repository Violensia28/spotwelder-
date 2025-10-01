#ifndef CONFIG_H
#define CONFIG_H

#define SSR_PIN 26
#define ACS712_PIN 34
#define ZMPT_PIN 35
#define BUZZER_PIN 27 // Dummy (Web Audio beep used)

#define PRESET_COUNT 99

#ifndef BUILD_VERSION
#define BUILD_VERSION "Build_3_Presets"
#endif

// SoftAP credentials
#define AP_SSID "SpotWelder_AP"
#define AP_PASS "12345678"

// OTA auth (basic HTTP auth)
#define OTA_USER "admin"
#define OTA_PASS "admin"

#endif

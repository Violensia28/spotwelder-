
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include "buildver_fix_autoshim.h"

WebServer server(80);
Preferences prefs;

uint8_t  opMode      = 0;     // 0=PRESET, 1=SMART
uint8_t  spotPattern = 0;     // 0=SINGLE, 1=DOUBLE
uint16_t gapMs       = 60;    // default gap

int   V_offset = 2048, I_offset = 2048;
float V_scale  = 0.10000f, I_scale = 0.00050f;

static const char* AP_SSID = "SpotWelder_AP";
static const char* AP_PASS = "12345678";

void setup(){
  Serial.begin(115200);
  delay(50);

  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(AP_SSID, AP_PASS, 1, false, 4);
  Serial.println(ok ? F("AP started") : F("AP start failed"));
  Serial.print(F("AP IP: ")); Serial.println(WiFi.softAPIP());
  Serial.print(F("Build: ")); Serial.println(F(BUILD_VERSION));

  server.on("/", HTTP_GET, [](){ server.send(200, "text/plain", "OK"); });
  server.on("/version", HTTP_GET, [](){ server.send(200, "text/plain", BUILD_VERSION); });

  server.begin();
}

void loop(){
  server.handleClient();
}

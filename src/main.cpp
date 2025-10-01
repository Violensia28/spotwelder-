#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "ui_assets_min.h"

#ifndef AP_ONLY
#define AP_ONLY 1
#endif

#ifndef DEFAULT_PIN_SSR
#define DEFAULT_PIN_SSR 26
#endif

WebServer server(80);

static void handleStatic(const char* mime, const char* data){ server.send_P(200, mime, data); }

static void setupRoutes(){
  server.on("/", []{ handleStatic("text/html; charset=utf-8", INDEX_HTML); });
  server.on("/css/main.css", []{ handleStatic("text/css", CSS_MAIN); });
  server.on("/css/theme.css", []{ handleStatic("text/css", CSS_THEME); });
  server.on("/js/app.js", []{ handleStatic("application/javascript", JS_APP); });
  server.on("/js/api.js", []{ handleStatic("application/javascript", JS_API); });
  server.on("/js/audio.js", []{ handleStatic("application/javascript", JS_AUDIO); });
  server.on("/js/ui.js", []{ handleStatic("application/javascript", JS_UI); });
  server.on("/js/preset.js", []{ handleStatic("application/javascript", JS_PRESET); });
  server.on("/js/manual.js", []{ handleStatic("application/javascript", JS_MANUAL); });
  server.on("/js/status.js", []{ handleStatic("application/javascript", JS_STATUS); });
  server.on("/js/logs.js", []{ handleStatic("application/javascript", JS_LOGS); });
  server.on("/js/settings.js", []{ handleStatic("application/javascript", JS_SETTINGS); });
  server.on("/js/ota.js", []{ handleStatic("application/javascript", JS_OTA); });
  server.on("/assets/logo.svg", []{ handleStatic("image/svg+xml", SVG_LOGO); });
  server.on("/ota", []{ handleStatic("text/html; charset=utf-8", OTA_HTML); });
  server.onNotFound([]{ server.send(404, "text/plain", "404"); });
}

void setup(){
  Serial.begin(115200);
  pinMode(DEFAULT_PIN_SSR, OUTPUT); digitalWrite(DEFAULT_PIN_SSR, LOW);
  WiFi.mode(WIFI_AP);
  WiFi.softAP("SPOTWELD+", "weld12345");
  setupRoutes();
  server.begin();
  Serial.println("Ready. Connect to AP SPOTWELD+ and open http://192.168.4.1/");
}

void loop(){ server.handleClient(); }

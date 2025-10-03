
#include <Arduino.h>
#include <WebServer.h>
#include <Preferences.h>
#include <pgmspace.h>
#include <stdio.h>
#include "buildver_fix_autoshim.h"

extern WebServer   server;
extern Preferences prefs;
extern uint8_t     opMode;
extern uint8_t     spotPattern;
extern uint16_t    gapMs;
extern int         V_offset;
extern int         I_offset;
extern float       V_scale;
extern float       I_scale;

#ifndef ZMPT_PIN
#define ZMPT_PIN -1
#endif
#ifndef ACS712_PIN
#define ACS712_PIN -1
#endif

static String patStr(uint8_t p){ return (p==1 ? String("DOUBLE") : String("SINGLE")); }
static String modeStr(uint8_t m){ return (m==1 ? String("SMART")  : String("PRESET")); }

static int avgAdc(int pin, int n=64){
  if (pin < 0) return -1;
  long s = 0; for (int i=0;i<n;i++) s += analogRead((uint8_t)pin);
  return (int)(s / (n>0?n:1));
}

void handleVersionJSON(){
  char buf[128];
  snprintf(buf, sizeof(buf), "{"build":"%s","op_mode":"%s","pattern":"%s","gap_ms":%u}",
           BUILD_VERSION, modeStr(opMode).c_str(), patStr(spotPattern).c_str(), (unsigned)gapMs);
  server.send(200, "application/json", String(buf));
}

void calibZeroV(){
  if (ZMPT_PIN < 0) { server.send(400, "text/plain", "ZMPT_PIN undefined"); return; }
  int newOff = avgAdc(ZMPT_PIN, 64);
  V_offset = newOff;
  prefs.begin("swp", false); prefs.putInt("v_off", V_offset); prefs.end();
  char buf[40]; snprintf(buf, sizeof(buf), "{"v_off":%d}", newOff);
  server.send(200, "application/json", String(buf));
}

void calibZeroI(){
  if (ACS712_PIN < 0) { server.send(400, "text/plain", "ACS712_PIN undefined"); return; }
  int newOff = avgAdc(ACS712_PIN, 64);
  I_offset = newOff;
  prefs.begin("swp", false); prefs.putInt("i_off", I_offset); prefs.end();
  char buf[40]; snprintf(buf, sizeof(buf), "{"i_off":%d}", newOff);
  server.send(200, "application/json", String(buf));
}

void calibSetScale(){
  bool changed=false;
  if (server.hasArg("v")) { V_scale = server.arg("v").toFloat(); changed=true; }
  if (server.hasArg("i")) { I_scale = server.arg("i").toFloat(); changed=true; }
  if (!changed) { server.send(400, "text/plain", "missing v/i"); return; }

  prefs.begin("swp", false);
  prefs.putFloat("v_sc", V_scale);
  prefs.putFloat("i_sc", I_scale);
  prefs.end();

  char buf[64]; snprintf(buf, sizeof(buf), "{"v_sc":%.5f,"i_sc":%.5f}", V_scale, I_scale);
  server.send(200, "application/json", String(buf));
}

extern "C" void initRoutes623(){
  server.on("/version.json",     HTTP_GET,  handleVersionJSON);
  server.on("/api/calib/zero_v", HTTP_POST, calibZeroV);
  server.on("/api/calib/zero_i", HTTP_POST, calibZeroI);
  server.on("/api/calib/scale",  HTTP_POST, calibSetScale);
}

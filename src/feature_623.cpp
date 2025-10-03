
#include <Arduino.h>
#include <WebServer.h>
#include <Preferences.h>
#include <pgmspace.h>
#include "buildver_fix_autoshim.h"  // ensure BUILD_VERSION is a string

// ===== Externals expected from existing firmware =====
extern WebServer   server;      // created in main.cpp
extern Preferences prefs;       // NVS namespace used in existing firmware
extern uint8_t     opMode;      // 0=PRESET, 1=SMART
extern uint8_t     spotPattern; // 0=SINGLE, 1=DOUBLE (6.2.2)
extern uint16_t    gapMs;       // weld gap ms (NVS)
extern int         V_offset;    // voltage ADC offset
extern int         I_offset;    // current ADC offset
extern float       V_scale;     // voltage scale
extern float       I_scale;     // current scale

#ifndef ZMPT_PIN
#define ZMPT_PIN -1
#endif
#ifndef ACS712_PIN
#define ACS712_PIN -1
#endif

// ===== Helpers =====
static String patStr(uint8_t p){ return (p==1 ? String("DOUBLE") : String("SINGLE")); }
static String modeStr(uint8_t m){ return (m==1 ? String("SMART")  : String("PRESET")); }

static int avgAdc(int pin, int n=64){
  if (pin < 0) return -1;
  long s = 0;
  for (int i = 0; i < n; i++) s += analogRead((uint8_t)pin);
  return (int)(s / (n > 0 ? n : 1));
}

// ===== Handlers 6.2.3 =====
void handleVersionJSON(){
  String out = "{";
  out += ""build":"";   out += BUILD_VERSION;       out += "",";
  out += ""op_mode":""; out += modeStr(opMode);     out += "",";
  out += ""pattern":""; out += patStr(spotPattern); out += "",";
  out += ""gap_ms":";     out += String((int)gapMs);
  out += "}";
  server.send(200, "application/json", out);
}

void calibZeroV(){
  if (ZMPT_PIN < 0) { server.send(400, "text/plain", "ZMPT_PIN undefined"); return; }
  int newOff = avgAdc(ZMPT_PIN, 64);
  V_offset = newOff;
  prefs.begin("swp", false); prefs.putInt("v_off", V_offset); prefs.end();
  String out = String("{"v_off":") + String(newOff) + String("}");
  server.send(200, "application/json", out);
}

void calibZeroI(){
  if (ACS712_PIN < 0) { server.send(400, "text/plain", "ACS712_PIN undefined"); return; }
  int newOff = avgAdc(ACS712_PIN, 64);
  I_offset = newOff;
  prefs.begin("swp", false); prefs.putInt("i_off", I_offset); prefs.end();
  String out = String("{"i_off":") + String(newOff) + String("}");
  server.send(200, "application/json", out);
}

void calibSetScale(){
  bool changed = false;
  if (server.hasArg("v")) { V_scale = server.arg("v").toFloat(); changed = true; }
  if (server.hasArg("i")) { I_scale = server.arg("i").toFloat(); changed = true; }
  if (!changed) { server.send(400, "text/plain", "missing v/i"); return; }

  prefs.begin("swp", false);
  prefs.putFloat("v_sc", V_scale);
  prefs.putFloat("i_sc", I_scale);
  prefs.end();

  String out = String("{"v_sc":") + String(V_scale, 5) + String(","i_sc":") + String(I_scale, 5) + String("}");
  server.send(200, "application/json", out);
}

// ===== Register routes (can be called anytime before server.begin()) =====
extern "C" void initRoutes623(){
  server.on("/version.json",     HTTP_GET,  handleVersionJSON);
  server.on("/api/calib/zero_v", HTTP_POST, calibZeroV);
  server.on("/api/calib/zero_i", HTTP_POST, calibZeroI);
  server.on("/api/calib/scale",  HTTP_POST, calibSetScale);
}

// ---- Auto-register before setup() so you don't need to edit main.cpp ----
// This relies on global static initialization order. server is already a global.
static struct _AutoReg623 { _AutoReg623(){ initRoutes623(); } } _autoReg623;

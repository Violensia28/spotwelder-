
#include <Arduino.h>
#include <WebServer.h>
#include <Preferences.h>
#include <pgmspace.h>
#include "buildver_fix_autoshim.h"  // memastikan BUILD_VERSION jadi string literal

// ===== Externals dari firmware utama =====
extern WebServer   server;      // didefinisikan di main.cpp
extern Preferences prefs;       // NVS
extern uint8_t     opMode;      // 0=PRESET, 1=SMART
extern uint8_t     spotPattern; // 0=SINGLE, 1=DOUBLE
extern uint16_t    gapMs;       // ms
extern int         V_offset;    // offset ADC V
extern int         I_offset;    // offset ADC I
extern float       V_scale;     // skala V
extern float       I_scale;     // skala I

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
  String out; out.reserve(24);
  out += F("{"v_off":"); out += newOff; out += '}';
  server.send(200, "application/json", out);
}

void calibZeroI(){
  if (ACS712_PIN < 0) { server.send(400, "text/plain", "ACS712_PIN undefined"); return; }
  int newOff = avgAdc(ACS712_PIN, 64);
  I_offset = newOff;
  prefs.begin("swp", false); prefs.putInt("i_off", I_offset); prefs.end();
  String out; out.reserve(24);
  out += F("{"i_off":"); out += newOff; out += '}';
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

  // Bangun JSON aman (tanpa raw literal panjang)
  String out; out.reserve(48);
  out += F("{"v_sc":");
  out += String(V_scale, 5);
  out += F(","i_sc":");
  out += String(I_scale, 5);
  out += '}';

  server.send(200, "application/json", out);
}

extern "C" void initRoutes623(){
  server.on("/version.json",     HTTP_GET,  handleVersionJSON);
  server.on("/api/calib/zero_v", HTTP_POST, calibZeroV);
  server.on("/api/calib/zero_i", HTTP_POST, calibZeroI);
  server.on("/api/calib/scale",  HTTP_POST, calibSetScale);
}

// (opsional) auto-register sebelum setup(); kalau tidak ingin, hapus blok berikut
static struct _AutoReg623 { _AutoReg623(){ initRoutes623(); } } _autoReg623;

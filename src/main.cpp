#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>
#include <math.h>
#include "Config.h"

// Forward declarations
void smartHistoryJson();
void smartHistoryCsv();

// Minimal globals to satisfy linker for this patch illustration
WebServer server(80);
Preferences prefs;

// AI history structures (as in Build 6.x)
struct AiHistEntry { uint32_t ms; float t_mm; uint16_t pre_ms; uint16_t main_ms; float E_est; float irms; char rating[8]; };
#ifndef AI_HIST_MAX
#define AI_HIST_MAX 32
#endif
AiHistEntry hist[AI_HIST_MAX];
uint8_t histCount=0, histHead=0; // assume elsewhere filled at runtime

// Implementations restored (missing in previous build)
void smartHistoryJson(){
  String out="["; int n=histCount; for(int i=0;i<n;i++){ int idx=(histHead - 1 - i + AI_HIST_MAX)%AI_HIST_MAX; const AiHistEntry &e=hist[idx]; if(i) out+=","; out+="{";
  out+="\"ms\":"+String(e.ms)+",";
  out+="\"t_mm\":"+String(e.t_mm,2)+",";
  out+="\"pre_ms\":"+String(e.pre_ms)+",";
  out+="\"main_ms\":"+String(e.main_ms)+",";
  out+="\"E_est\":"+String(e.E_est,1)+",";
  out+="\"irms\":"+String(e.irms,2)+",";
  out+="\"rating\":\""+String(e.rating)+"\"";
  out+="}"; } out+="]"; server.send(200,"application/json",out);
}

void smartHistoryCsv(){
  String out="ms,t_mm,pre_ms,main_ms,E_est,irms,rating\n"; int n=histCount; for(int i=0;i<n;i++){ int idx=(histHead - 1 - i + AI_HIST_MAX)%AI_HIST_MAX; const AiHistEntry &e=hist[idx];
  out+=String(e.ms)+","+String(e.t_mm,2)+","+String(e.pre_ms)+","+String(e.main_ms)+","+String(e.E_est,1)+","+String(e.irms,2)+","+String(e.rating)+"\n"; }
  server.send(200,"text/csv",out);
}

void setup(){ Serial.begin(115200); server.on("/smart/history", HTTP_GET, smartHistoryJson); server.on("/smart/history.csv", HTTP_GET, smartHistoryCsv); server.begin(); }
void loop(){ server.handleClient(); }

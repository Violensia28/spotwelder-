// main.cpp (with Hotfix 7.0.1 applied)
#include <Arduino.h>
#include <WebServer.h>

// ... (other includes and global variables)

WebServer server(80);

// Assume AiHistEntry and related variables are declared somewhere above
struct AiHistEntry {
    uint32_t ms;
    float t_mm;
    uint16_t pre_ms;
    uint16_t main_ms;
    float E_est;
    float irms;
    char rating[8];
};

extern AiHistEntry hist[];
extern uint8_t histCount, histHead;
#define AI_HIST_MAX 32

void setup() {
    // ... existing setup code ...

    // Ensure these routes exist
    server.on("/smart/history", HTTP_GET, smartHistoryJson);
    server.on("/smart/history.csv", HTTP_GET, smartHistoryCsv);

    // ... rest of setup ...
}

void loop() {
    server.handleClient();
    // ... rest of loop ...
}

// ==== HOTFIX 7.0.1: Restore Smart History Endpoints ====
void smartHistoryJson(){
  String out = "[";
  int n = histCount;
  for(int i=0;i<n;i++){
    int idx = (histHead - 1 - i + AI_HIST_MAX) % AI_HIST_MAX;
    const AiHistEntry &e = hist[idx];
    if(i) out += ",";
    out += "{";
    out += "\"ms\":"        + String(e.ms) + ",";
    out += "\"t_mm\":"      + String(e.t_mm, 2) + ",";
    out += "\"pre_ms\":"    + String(e.pre_ms) + ",";
    out += "\"main_ms\":"   + String(e.main_ms) + ",";
    out += "\"E_est\":"     + String(e.E_est, 1) + ",";
    out += "\"irms\":"      + String(e.irms, 2) + ",";
    out += "\"rating\":\""  + String(e.rating) + "\"";
    out += "}";
  }
  out += "]";
  server.send(200, "application/json", out);
}

void smartHistoryCsv(){
  String out = "ms,t_mm,pre_ms,main_ms,E_est,irms,rating\n";
  int n = histCount;
  for(int i=0;i<n;i++){
    int idx = (histHead - 1 - i + AI_HIST_MAX) % AI_HIST_MAX;
    const AiHistEntry &e = hist[idx];
    out += String(e.ms) + "," + String(e.t_mm,2) + "," +
           String(e.pre_ms) + "," + String(e.main_ms) + "," +
           String(e.E_est,1) + "," + String(e.irms,2) + "," +
           String(e.rating) + "\n";
  }
  server.send(200, "text/csv", out);
}
// ==== END HOTFIX 7.0.1 ====

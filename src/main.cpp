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

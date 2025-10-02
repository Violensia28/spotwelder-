
#include <Arduino.h>
#include <WebServer.h>

extern WebServer server; // already defined in your project

// Minimal implementations so build succeeds without changing your logic
void smartHistoryJson(){
  // Return empty list if your code doesn't push history yet
  server.send(200, "application/json", "[]");
}

void smartHistoryCsv(){
  // Return only CSV header line (no rows)
  server.send(200, "text/csv", "ms,t_mm,pre_ms,main_ms,E_est,irms,rating
");
}

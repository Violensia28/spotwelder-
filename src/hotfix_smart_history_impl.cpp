
#include <Arduino.h>
#include <WebServer.h>

extern WebServer server; // defined in your project

void smartHistoryJson(){
  server.send(200, "application/json", "[]");
}

void smartHistoryCsv(){
  // PROGMEM single-line literal to avoid accidental line breaks
  static const char CSV_HEADER[] PROGMEM = "ms,t_mm,pre_ms,main_ms,E_est,irms,rating
";
  server.send_P(200, "text/csv", CSV_HEADER);
}

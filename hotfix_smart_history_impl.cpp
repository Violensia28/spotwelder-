
#include <Arduino.h>
#include <WebServer.h>

extern WebServer server; // defined in your project

void smartHistoryJson(){
  server.send(200, "application/json", "[]");
}

void smartHistoryCsv(){
  // Build CSV header using short literals to avoid any line-wrap issues
  String out; out.reserve(64);
  out += "ms"; out += ","; out += "t_mm"; out += ","; out += "pre_ms"; out += ",";
  out += "main_ms"; out += ","; out += "E_est"; out += ","; out += "irms"; out += ","; out += "rating"; out += "
";
  server.send(200, "text/csv", out);
}

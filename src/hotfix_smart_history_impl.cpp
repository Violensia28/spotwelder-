#include <Arduino.h>
#include <WebServer.h>

extern WebServer server; // sudah didefinisikan di project-mu

// JSON: kembalikan list kosong agar UI tidak error
void smartHistoryJson() {
  server.send(200, "application/json", "[]");
}

// CSV: hanya header, tanpa baris (aman & minimal)
void smartHistoryCsv() {
  static const char* header = "ms,t_mm,pre_ms,main_ms,E_est,irms,rating
";
  server.send(200, "text/csv", header);
}

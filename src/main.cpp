#include <WiFi.h>
#include <WebServer.h>
#include "Config.h"

const char* ssid = "SpotWelder_AP";
const char* password = "12345678";

WebServer server(80);

// Simple HTML UI with Web Audio beep
const char htmlPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<title>SpotWelder+</title>
<script>
function beep() {
    var ctx = new (window.AudioContext || window.webkitAudioContext)();
    var oscillator = ctx.createOscillator();
    oscillator.type = 'square';
    oscillator.frequency.setValueAtTime(440, ctx.currentTime);
    oscillator.connect(ctx.destination);
    oscillator.start();
    setTimeout(function(){ oscillator.stop(); }, 200);
}
</script>
</head>
<body>
<h1>SpotWelder+ Control</h1>
<button onclick="fetch('/trigger'); beep();">Trigger Weld</button>
</body>
</html>
)rawliteral";

void handleRoot() {
    server.send(200, "text/html", htmlPage);
}

void handleTrigger() {
    digitalWrite(SSR_PIN, HIGH);
    delay(100); // simulate weld pulse
    digitalWrite(SSR_PIN, LOW);
    server.send(200, "text/plain", "Weld triggered");
}

void setup() {
    Serial.begin(115200);
    pinMode(SSR_PIN, OUTPUT);
    digitalWrite(SSR_PIN, LOW);

    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, password);
    Serial.println("AP started: SpotWelder+");

    server.on("/", handleRoot);
    server.on("/trigger", handleTrigger);
    server.begin();
}

void loop() {
    server.handleClient();
}

#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include "Config.h"

WebServer server(80);

static const char indexHtml[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>SpotWelder+ %v%</title>
<style>
:root{--bg:#0b132b;--card:#1b2a49;--fg:#e0e0e0;--btn:#1c2541;--accent:#5bc0be}
body{font-family:system-ui,Segoe UI,Roboto,Arial;margin:24px;background:var(--bg);color:var(--fg)}
button,a.button{font-size:18px;padding:12px 18px;border-radius:10px;border:none;background:var(--btn);color:#fff;cursor:pointer;text-decoration:none;display:inline-block}
button:active{transform:scale(.98)}
.card{background:var(--card);padding:20px;border-radius:14px;max-width:520px}
small{opacity:.7}
hr{border:0;border-top:1px solid #2a3b63;margin:16px 0}
.badge{display:inline-block;padding:4px 8px;border-radius:6px;background:#0f3460;}
</style>
<script>
function beep(){try{const c=new (window.AudioContext||window.webkitAudioContext)();const o=c.createOscillator();const g=c.createGain();o.type='square';o.frequency.value=720;g.gain.value=0.05;o.connect(g);g.connect(c.destination);o.start();setTimeout(()=>o.stop(),160);}catch(e){}}
async function trig(){const r=await fetch('/trigger'); const t=await r.text(); beep(); document.getElementById('status').textContent=t;}
</script>
</head>
<body>
  <h2>SpotWelder+ <span class="badge">%v%</span></h2>
  <div class="card">
    <p>Minimal Web UI: <b>Trigger</b> + Web Audio Beep</p>
    <button onclick="trig()">Trigger Weld</button>
    <p id="status"><small>Ready.</small></p>
    <hr/>
    <p>Firmware Update (OTA):</p>
    <a class="button" href="/update">Open OTA Updater</a>
    <p><small>Tips: unggah file <code>spotweldingplus-app.bin</code> dari hasil CI/Release.</small></p>
  </div>
</body>
</html>
)rawliteral";

static const char updateHtml[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>OTA Update</title>
<style>
body{font-family:system-ui,Segoe UI,Roboto,Arial;margin:24px;background:#0b132b;color:#e0e0e0}
.card{background:#1b2a49;padding:20px;border-radius:14px;max-width:520px}
input[type=file]{background:#0f3460;padding:10px;border-radius:8px}
button{font-size:16px;padding:10px 16px;border-radius:8px;border:none;background:#1c2541;color:#fff;cursor:pointer}
progress{width:100%}
</style>
</head>
<body>
  <h2>OTA Update</h2>
  <div class="card">
    <form method="POST" action="/update" enctype="multipart/form-data">
      <p>Pilih file firmware (<code>.bin</code>) lalu klik <b>Upload</b>.</p>
      <input type="file" name="firmware" accept=".bin" required>
      <button type="submit">Upload</button>
    </form>
    <p><small>Gunakan file: <code>spotweldingplus-app.bin</code> (hasil CI/Release)</small></p>
  </div>
</body>
</html>
)rawliteral";

String applyVersion(const char* tpl){
  String s(tpl); s.replace("%v%", BUILD_VERSION); return s;
}

void handleRoot(){ server.send(200, "text/html", applyVersion(indexHtml)); }

void handleTrigger(){
  digitalWrite(SSR_PIN, HIGH);
  delay(100);
  digitalWrite(SSR_PIN, LOW);
  server.send(200, "text/plain", "Weld triggered");
}

void handleUpdateGet(){
  if(!server.authenticate(OTA_USER, OTA_PASS)) return server.requestAuthentication();
  server.send(200, "text/html", updateHtml);
}

void handleUpdatePost(){
  if(!server.authenticate(OTA_USER, OTA_PASS)) return server.requestAuthentication();
  bool ok = !Update.hasError();
  server.sendHeader("Connection", "close");
  server.send(200, "text/plain", ok ? "OK" : "FAIL");
  delay(200);
  ESP.restart();
}

void handleUpdateUpload(){
  if(!server.authenticate(OTA_USER, OTA_PASS)) return; // no body response here
  HTTPUpload& upload = server.upload();
  if(upload.status == UPLOAD_FILE_START){
    Serial.printf("[OTA] Update start: %s\n", upload.filename.c_str());
    if(!Update.begin(UPDATE_SIZE_UNKNOWN)){
      Update.printError(Serial);
    }
  } else if(upload.status == UPLOAD_FILE_WRITE){
    if(Update.write(upload.buf, upload.currentSize) != upload.currentSize){
      Update.printError(Serial);
    }
  } else if(upload.status == UPLOAD_FILE_END){
    if(Update.end(true)){
      Serial.printf("[OTA] Update success: %u bytes\n", upload.totalSize);
    } else {
      Update.printError(Serial);
    }
  } else if(upload.status == UPLOAD_FILE_ABORTED){
    Update.end();
    Serial.println("[OTA] Update aborted");
  }
}

void setup(){
  Serial.begin(115200);
  pinMode(SSR_PIN, OUTPUT); digitalWrite(SSR_PIN, LOW);

  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 4);
  Serial.println(ok ? "AP started: SpotWelder+" : "AP start failed!");
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
  Serial.printf("Build: %s\n", BUILD_VERSION);

  server.on("/", handleRoot);
  server.on("/trigger", handleTrigger);
  server.on("/version", [](){ server.send(200, "text/plain", BUILD_VERSION); });
  server.on("/update", HTTP_GET, handleUpdateGet);
  server.on("/update", HTTP_POST, handleUpdatePost, handleUpdateUpload);
  server.onNotFound(handleRoot);
  server.begin();
}

void loop(){ server.handleClient(); }

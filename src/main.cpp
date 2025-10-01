#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>
#include "Config.h"

// ====== Globals ======
WebServer server(80);
Preferences prefs; // NVS storage

// Current selection
volatile uint8_t g_selectedPreset = 1; // 1..99

// Weld FSM
enum WeldState { IDLE, PRE_PULSE, GAP, MAIN_PULSE };
volatile WeldState g_state = IDLE;
unsigned long g_t0 = 0;
uint16_t g_preMs = 0, g_gapMs = 60, g_mainMs = 100; // default

// ====== Preset model ======
struct Preset { uint8_t id; const char* name; const char* group; uint16_t preMs; uint16_t mainMs; };

// Generate pseudo presets at runtime to save PROGMEM size
// Groups: Ni-Thin, Ni-Med, Ni-Thick, Al, Cu, Custom
static const char* GRP_NI_THIN = "Ni-Thin";
static const char* GRP_NI_MED  = "Ni-Med";
static const char* GRP_NI_THK  = "Ni-Thick";
static const char* GRP_AL      = "Al";
static const char* GRP_CU      = "Cu";
static const char* GRP_CUSTOM  = "Custom";

static const char* groupForLevel(uint8_t L){
  if(L <= 20) return GRP_NI_THIN;     // 1-20
  if(L <= 40) return GRP_NI_MED;      // 21-40
  if(L <= 60) return GRP_NI_THK;      // 41-60
  if(L <= 78) return GRP_AL;          // 61-78
  if(L <= 96) return GRP_CU;          // 79-96
  return GRP_CUSTOM;                   // 97-99
}

// Simple mapping from level -> durations (example curve)
static void durationsForLevel(uint8_t L, uint16_t &pre, uint16_t &main){
  // Preheat grows 0..80 ms (mild until mid, then quicker)
  pre  = (uint16_t)( (L<=60) ? (L*1.0) : (60 + (L-60)*2.0) );
  if(pre>80) pre=80;
  // Main grows 70..300 ms roughly linear with slight bias after 60
  main = (uint16_t)( 70 + (L* (L<=60 ? 2.0 : 2.6)) );
  if(main>300) main=300;
}

static void makePreset(uint8_t L, Preset &p){
  static char nameBuf[16];
  snprintf(nameBuf, sizeof(nameBuf), "L%02u", L);
  p.id = L; p.name = nameBuf; p.group = groupForLevel(L);
  durationsForLevel(L, p.preMs, p.mainMs);
}

// ====== HTML (Index + Preset UI + OTA link) ======
static const char indexHtml[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>SpotWelder+ %v%</title>
<style>
:root{--bg:#0b132b;--card:#1b2a49;--fg:#e0e0e0;--btn:#1c2541;--accent:#5bc0be}
*{box-sizing:border-box}
body{font-family:system-ui,Segoe UI,Roboto,Arial;margin:20px;background:var(--bg);color:var(--fg)}
.btn{font-size:16px;padding:10px 14px;border-radius:10px;border:none;background:var(--btn);color:#fff;cursor:pointer;text-decoration:none;display:inline-block}
.btn:active{transform:scale(.98)}
.card{background:var(--card);padding:16px;border-radius:14px;max-width:920px;margin:0 auto}
.row{display:flex;gap:12px;flex-wrap:wrap}
.col{flex:1 1 260px}
.badge{display:inline-block;padding:4px 8px;border-radius:6px;background:#0f3460;}
select,input{padding:8px;border-radius:8px;border:1px solid #2a3b63;background:#0f1b3d;color:#fff}
table{width:100%;border-collapse:collapse;margin-top:8px}
th,td{border-bottom:1px solid #2a3b63;padding:8px;text-align:left}
tr:hover{background:#0f1f3e}
small{opacity:.7}
hr{border:0;border-top:1px solid #2a3b63;margin:14px 0}
</style>
<script>
let presets=[]; let view=[]; let current=1; let grp=''; let q='';
async function load(){
  const pv = await fetch('/api/presets'); presets=await pv.json();
  const cv = await fetch('/api/current'); const cx = await cv.json(); current=cx.id;
  document.getElementById('ver').textContent='%v%';
  render();
}
function render(){
  const tbody = document.getElementById('tbody');
  tbody.innerHTML='';
  view = presets.filter(p=> (grp? p.group===grp:true) && (q? (p.name.toLowerCase().includes(q)||String(p.id).includes(q)) : true));
  for(const p of view){
    const tr = document.createElement('tr');
    tr.innerHTML = `<td>${p.id}</td><td>${p.name}</td><td>${p.group}</td><td>${p.preMs} ms</td><td>${p.mainMs} ms</td>`+
      `<td><button class="btn" onclick="sel(${p.id})">Select</button></td>`;
    if(p.id===current){ tr.style.outline='2px solid var(--accent)'; }
    tbody.appendChild(tr);
  }
  document.getElementById('count').textContent=view.length;
  document.getElementById('cur').textContent=current;
}
async function sel(id){
  const r = await fetch('/api/select?id='+id); if(r.ok){ current=id; render(); beep(); }
}
function setGrp(v){ grp=v; render(); }
function setQ(v){ q=v.toLowerCase(); render(); }
function beep(){try{const c=new (window.AudioContext||window.webkitAudioContext)();const o=c.createOscillator();const g=c.createGain();o.type='square';o.frequency.value=720;g.gain.value=0.05;o.connect(g);g.connect(c.destination);o.start();setTimeout(()=>o.stop(),160);}catch(e){}}
async function trig(){ const r=await fetch('/trigger'); const t=await r.text(); beep(); document.getElementById('status').textContent=t; }
</script>
</head>
<body onload="load()">
  <div class="card">
    <h2>SpotWelder+ <span class="badge" id="ver"></span></h2>
    <div class="row">
      <div class="col">
        <p><b>Trigger</b> (pakai preset terpilih: <span id="cur">-</span>)</p>
        <button class="btn" onclick="trig()">Trigger Weld</button>
        <p id="status"><small>Ready.</small></p>
        <hr>
        <p>OTA Update:</p>
        <a class="btn" href="/update">Open OTA Updater</a>
      </div>
      <div class="col">
        <p><b>Filter Preset</b></p>
        <select onchange="setGrp(this.value)">
          <option value="">All</option>
          <option>Ni-Thin</option>
          <option>Ni-Med</option>
          <option>Ni-Thick</option>
          <option>Al</option>
          <option>Cu</option>
          <option>Custom</option>
        </select>
        <p style="margin-top:8px"><input placeholder="Cari id/nama..." oninput="setQ(this.value)"></p>
        <small>Tampil: <span id="count">0</span> item</small>
      </div>
    </div>
    <hr>
    <table>
      <thead><tr><th>ID</th><th>Nama</th><th>Grup</th><th>Pre</th><th>Main</th><th></th></tr></thead>
      <tbody id="tbody"></tbody>
    </table>
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

// ====== Helpers ======
String applyVersion(const char* tpl){ String s(tpl); s.replace("%v%", BUILD_VERSION); return s; }

void respondJsonPresets(){
  String out = "[";
  for(uint8_t i=1;i<=PRESET_COUNT;i++){
    Preset p; makePreset(i,p);
    if(i>1) out += ",";
    out += "{";
    out += "\"id\":" + String(p.id) + ",";
    out += "\"name\":\"" + String(p.name) + "\",";
    out += "\"group\":\"" + String(p.group) + "\",";
    out += "\"preMs\":" + String(p.preMs) + ",";
    out += "\"mainMs\":" + String(p.mainMs);
    out += "}";
  }
  out += "]";
  server.send(200, "application/json", out);
}

void loadSelected(){
  prefs.begin("swp", true);
  g_selectedPreset = prefs.getUChar("sel", 1);
  prefs.end();
  // apply durations
  Preset p; makePreset(g_selectedPreset, p);
  g_preMs = p.preMs; g_mainMs = p.mainMs; g_gapMs = 60; // fixed gap for now
}

void saveSelected(){
  prefs.begin("swp", false);
  prefs.putUChar("sel", g_selectedPreset);
  prefs.end();
}

// ====== Routes ======
void handleRoot(){ server.send(200, "text/html", applyVersion(indexHtml)); }
void handleUpdateGet(){ if(!server.authenticate(OTA_USER, OTA_PASS)) return server.requestAuthentication(); server.send(200, "text/html", updateHtml); }
void handleUpdatePost(){ if(!server.authenticate(OTA_USER, OTA_PASS)) return server.requestAuthentication(); bool ok = !Update.hasError(); server.sendHeader("Connection","close"); server.send(200, "text/plain", ok?"OK":"FAIL"); delay(200); ESP.restart(); }
void handleUpdateUpload(){ if(!server.authenticate(OTA_USER, OTA_PASS)) return; HTTPUpload& u=server.upload(); if(u.status==UPLOAD_FILE_START){ if(!Update.begin(UPDATE_SIZE_UNKNOWN)){ Update.printError(Serial);} } else if(u.status==UPLOAD_FILE_WRITE){ if(Update.write(u.buf,u.currentSize)!=u.currentSize){ Update.printError(Serial);} } else if(u.status==UPLOAD_FILE_END){ if(Update.end(true)){ Serial.printf("[OTA] %u bytes\n", u.totalSize);} else { Update.printError(Serial);} } else if(u.status==UPLOAD_FILE_ABORTED){ Update.end(); Serial.println("[OTA] aborted"); } }

void handlePresets(){ respondJsonPresets(); }
void handleCurrent(){ server.send(200, "application/json", String("{\"id\":"+String(g_selectedPreset)+"}")); }
void handleSelect(){ if(!server.hasArg("id")){ server.send(400, "text/plain", "missing id"); return;} int id = server.arg("id").toInt(); if(id<1||id>99){ server.send(400, "text/plain", "bad id"); return;} g_selectedPreset = (uint8_t)id; saveSelected(); Preset p; makePreset(g_selectedPreset,p); g_preMs = p.preMs; g_mainMs = p.mainMs; server.send(200, "text/plain", "OK"); }

// ====== Trigger (non-blocking FSM) ======
void startWeld(){ if(g_state!=IDLE) return; g_state = (g_preMs>0? PRE_PULSE: MAIN_PULSE); g_t0 = millis(); digitalWrite(SSR_PIN, (g_state!=IDLE)); }

void fsmLoop(){ unsigned long t = millis(); switch(g_state){
  case IDLE: break;
  case PRE_PULSE:
    if(t - g_t0 >= g_preMs){ digitalWrite(SSR_PIN, LOW); g_state = GAP; g_t0 = t; }
    break;
  case GAP:
    if(t - g_t0 >= g_gapMs){ g_state = MAIN_PULSE; g_t0 = t; digitalWrite(SSR_PIN, HIGH); }
    break;
  case MAIN_PULSE:
    if(t - g_t0 >= g_mainMs){ digitalWrite(SSR_PIN, LOW); g_state = IDLE; }
    break; }
}

void handleTrigger(){ startWeld(); server.send(200, "text/plain", "Weld cycle started"); }

// ====== Setup / Loop ======
void setup(){
  Serial.begin(115200);
  pinMode(SSR_PIN, OUTPUT); digitalWrite(SSR_PIN, LOW);
  loadSelected();

  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 4);
  Serial.println(ok ? "AP started: SpotWelder+" : "AP start failed!");
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
  Serial.printf("Build: %s\n", BUILD_VERSION);

  server.on("/", handleRoot);
  server.on("/ui", handleRoot);
  server.on("/trigger", handleTrigger);
  server.on("/version", [](){ server.send(200, "text/plain", BUILD_VERSION); });
  server.on("/api/presets", HTTP_GET, handlePresets);
  server.on("/api/current", HTTP_GET, handleCurrent);
  server.on("/api/select", HTTP_GET, handleSelect);
  server.on("/update", HTTP_GET, handleUpdateGet);
  server.on("/update", HTTP_POST, handleUpdatePost, handleUpdateUpload);
  // alias & redirects for convenience
  server.on("/update/", HTTP_GET, [](){ server.sendHeader("Location","/update"); server.send(302, "text/plain", ""); });
  server.on("/admin", HTTP_GET, [](){ server.sendHeader("Location","/update"); server.send(302, "text/plain", ""); });
  server.on("/ota", HTTP_GET, [](){ server.sendHeader("Location","/update"); server.send(302, "text/plain", ""); });
  server.onNotFound(handleRoot);
  server.begin();
}

void loop(){ server.handleClient(); fsmLoop(); }

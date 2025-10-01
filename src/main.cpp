#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>
#include <math.h>
#include "Config.h"

// ================= Globals =================
WebServer server(80);
Preferences prefs; // NVS

// Preset selection
volatile uint8_t g_selectedPreset = 1; // 1..99

// Weld FSM
enum WeldState { IDLE, PRE_PULSE, GAP, MAIN_PULSE };
volatile WeldState g_state = IDLE;
unsigned long g_t0 = 0;
uint16_t g_preMs = 0, g_gapMs = 60, g_mainMs = 100; // defaults

// Sensor calibration (raw offsets and scales)
int32_t  I_offset = DEF_I_OFFSET;
int32_t  V_offset = DEF_V_OFFSET;
float    I_scale  = DEF_I_SCALE; // A per count
float    V_scale  = DEF_V_SCALE; // V per count

// Sensor accumulation window
struct Accum { uint64_t sumSq; int64_t sum; uint32_t n; };
Accum accI = {0,0,0};
Accum accV = {0,0,0};
uint32_t winStart = 0;
float Irms = 0.0f, Vrms = 0.0f; // computed every window

// Auto-Trigger & Guards
bool  auto_en   = DEF_AUTO_EN;
bool  guard_en  = DEF_GUARD_EN;
float I_trig_A  = DEF_I_TRIG_A;
float V_cut_V   = DEF_V_CUTOFF_V;
float I_lim_A   = DEF_I_LIMIT_A;
uint32_t cooldownMs = DEF_COOLDOWN_MS;
uint8_t stableWinReq = DEF_STABLE_WIN;

uint8_t stableCnt = 0;         // consecutive windows over thresholds
unsigned long lastWeldEnd = 0; // timestamp of last cycle end

// Event log (simple ring)
static const int LOGN = 20;
String logBuf[LOGN];
int logIdx = 0;

void addLog(const String &s){ logBuf[logIdx] = s; logIdx = (logIdx+1)%LOGN; Serial.println(String("[LOG] ")+s); }

// ================= Preset model =================
struct Preset { uint8_t id; const char* name; const char* group; uint16_t preMs; uint16_t mainMs; };
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

static void durationsForLevel(uint8_t L, uint16_t &pre, uint16_t &main){
  pre  = (uint16_t)( (L<=60) ? (L*1.0) : (60 + (L-60)*2.0) );
  if(pre>80) pre=80;
  main = (uint16_t)( 70 + (L* (L<=60 ? 2.0 : 2.6)) );
  if(main>300) main=300;
}

static void makePreset(uint8_t L, Preset &p){
  static char nameBuf[16];
  snprintf(nameBuf, sizeof(nameBuf), "L%02u", L);
  p.id = L; p.name = nameBuf; p.group = groupForLevel(L);
  durationsForLevel(L, p.preMs, p.mainMs);
}

// ================= HTML UI =================
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
input,select{padding:8px;border-radius:8px;border:1px solid #2a3b63;background:#0f1b3d;color:#fff}
.card{background:var(--card);padding:16px;border-radius:14px;max-width:1020px;margin:0 auto}
.row{display:flex;gap:12px;flex-wrap:wrap}
.col{flex:1 1 300px}
.badge{display:inline-block;padding:4px 8px;border-radius:6px;background:#0f3460;}
small{opacity:.7}
hr{border:0;border-top:1px solid #2a3b63;margin:14px 0}
table{width:100%;border-collapse:collapse;margin-top:8px}
th,td{border-bottom:1px solid #2a3b63;padding:8px;text-align:left}
.switch{display:inline-flex;align-items:center;gap:8px}
</style>
<script>
let presets=[]; let current=1; let grp=''; let q='';
async function load(){
  const pv = await fetch('/api/presets'); presets=await pv.json();
  const cv = await fetch('/api/current'); const cx = await cv.json(); current=cx.id;
  await loadGuard();
  document.getElementById('ver').textContent='%v%';
  renderPresets();
  poll(); setInterval(poll, 600);
}
function renderPresets(){
  const tbody = document.getElementById('tbody'); tbody.innerHTML='';
  const view = presets.filter(p=> (grp? p.group===grp:true) && (q? (p.name.toLowerCase().includes(q)||String(p.id).includes(q)) : true));
  for(const p of view){
    const tr = document.createElement('tr');
    tr.innerHTML = `<td>${p.id}</td><td>${p.name}</td><td>${p.group}</td><td>${p.preMs} ms</td><td>${p.mainMs} ms</td>`+
      `<td><button class='btn' onclick='sel(${p.id})'>Select</button></td>`;
    if(p.id===current){ tr.style.outline='2px solid var(--accent)'; }
    tbody.appendChild(tr);
  }
  document.getElementById('cur').textContent=current;
}
async function sel(id){ const r=await fetch('/api/select?id='+id); if(r.ok){ current=id; renderPresets(); beep(); } }
function setGrp(v){ grp=v; renderPresets(); }
function setQ(v){ q=v.toLowerCase(); renderPresets(); }

async function poll(){
  try{
    const r = await fetch('/api/sensor'); const s = await r.json();
    id('vrms').textContent = s.vrms.toFixed(1)+' V';
    id('irms').textContent = s.irms.toFixed(2)+' A';
    id('samples').textContent = s.n + ' samp / '+ s.win_ms +' ms';
    const g = await (await fetch('/api/guard/status')).json();
    id('auto_state').textContent = g.auto_en?'ON':'OFF';
    id('guard_state').textContent = g.guard_en?'ON':'OFF';
    id('cool').textContent = g.cooldown_ms+' ms';
    id('stable').textContent = g.stable_win+' win';
    id('ready').textContent = g.ready? 'READY' : 'HOLD';
    id('last').textContent = g.last;
  }catch(e){console.log(e)}
}

async function loadGuard(){
  const g = await (await fetch('/api/guard/load')).json();
  id('auto_en').checked = g.auto_en; id('guard_en').checked = g.guard_en;
  id('i_trig').value = g.i_trig.toFixed(2);
  id('v_cut').value = g.v_cut.toFixed(1);
  id('i_lim').value = g.i_lim.toFixed(1);
  id('cd').value = g.cooldown_ms;
  id('sw').value = g.stable_win;
}
async function saveGuard(){
  const p = new URLSearchParams({
    auto: id('auto_en').checked? '1':'0',
    guard: id('guard_en').checked? '1':'0',
    i_trig: id('i_trig').value,
    v_cut: id('v_cut').value,
    i_lim: id('i_lim').value,
    cd: id('cd').value,
    sw: id('sw').value
  });
  const r = await fetch('/api/guard/save?'+p.toString()); if(r.ok){ beep(); await loadGuard(); }
}

function id(s){return document.getElementById(s)}
function beep(){try{const c=new (window.AudioContext||window.webkitAudioContext)();const o=c.createOscillator();const g=c.createGain();o.type='square';o.frequency.value=720;g.gain.value=0.05;o.connect(g);g.connect(c.destination);o.start();setTimeout(()=>o.stop(),140);}catch(e){}}
async function trig(){ const r=await fetch('/trigger'); const t=await r.text(); beep(); id('status').textContent=t; }
</script>
</head>
<body onload="load()">
  <div class="card">
    <h2>SpotWelder+ <span class="badge" id="ver"></span></h2>
    <div class="row">
      <div class="col">
        <p><b>Trigger</b> (preset: <span id="cur">-</span>)</p>
        <button class="btn" onclick="trig()">Trigger Weld</button>
        <p id="status"><small>Ready.</small></p>
        <hr>
        <p>OTA Update:</p>
        <a class="btn" href="/update">Open OTA Updater</a>
      </div>
      <div class="col">
        <p><b>Live Sensor</b></p>
        <div>Vrms: <b id="vrms">-</b></div>
        <div>Irms: <b id="irms">-</b></div>
        <small id="samples">-</small>
      </div>
      <div class="col">
        <p><b>Auto‑Trigger & Guard</b></p>
        <div class="switch"><input type="checkbox" id="auto_en"> <label for="auto_en">Auto‑Trigger</label></div>
        <div class="switch"><input type="checkbox" id="guard_en"> <label for="guard_en">V/I Guard</label></div>
        <div style="margin-top:8px">
          <div>I trig (A): <input id="i_trig" style="width:120px"></div>
          <div>V cutoff (V): <input id="v_cut" style="width:120px"></div>
          <div>I limit (A): <input id="i_lim" style="width:120px"></div>
          <div>Cooldown (ms): <input id="cd" style="width:120px"></div>
          <div>Stable windows: <input id="sw" style="width:120px"></div>
        </div>
        <p style="margin-top:8px"><button class="btn" onclick="saveGuard()">Save Guard</button></p>
        <small>Status: auto=<b id="auto_state">-</b>, guard=<b id="guard_state">-</b>, ready=<b id="ready">-</b>, cd=<b id="cool">-</b>, sw=<b id="stable">-</b></small>
        <div style="margin-top:6px"><small>Last: <span id="last">-</span></small></div>
      </div>
    </div>
    <hr>
    <h3>Preset Table</h3>
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

// ================= Helpers =================
String applyVersion(const char* tpl){ String s(tpl); s.replace("%v%", BUILD_VERSION); return s; }

void loadSelected(){
  prefs.begin("swp", true);
  g_selectedPreset = prefs.getUChar("sel", 1);
  // calib
  I_offset = prefs.getInt("i_off", DEF_I_OFFSET);
  V_offset = prefs.getInt("v_off", DEF_V_OFFSET);
  I_scale  = prefs.getFloat("i_sc", DEF_I_SCALE);
  V_scale  = prefs.getFloat("v_sc", DEF_V_SCALE);
  // guard
  auto_en  = prefs.getBool("auto_en", DEF_AUTO_EN);
  guard_en = prefs.getBool("grd_en", DEF_GUARD_EN);
  I_trig_A = prefs.getFloat("i_trig", DEF_I_TRIG_A);
  V_cut_V  = prefs.getFloat("v_cut", DEF_V_CUTOFF_V);
  I_lim_A  = prefs.getFloat("i_lim", DEF_I_LIMIT_A);
  cooldownMs = prefs.getUInt("cool_ms", DEF_COOLDOWN_MS);
  stableWinReq = (uint8_t)prefs.getUChar("stb_win", DEF_STABLE_WIN);
  prefs.end();
  // apply durations from preset
  Preset p; makePreset(g_selectedPreset, p);
  g_preMs = p.preMs; g_mainMs = p.mainMs; g_gapMs = 60;
}

void saveSelected(){ prefs.begin("swp", false); prefs.putUChar("sel", g_selectedPreset); prefs.end(); }

void saveCalib(){ prefs.begin("swp", false); prefs.putInt("i_off", I_offset); prefs.putInt("v_off", V_offset); prefs.putFloat("i_sc", I_scale); prefs.putFloat("v_sc", V_scale); prefs.end(); }

void saveGuard(){ prefs.begin("swp", false); prefs.putBool("auto_en", auto_en); prefs.putBool("grd_en", guard_en); prefs.putFloat("i_trig", I_trig_A); prefs.putFloat("v_cut", V_cut_V); prefs.putFloat("i_lim", I_lim_A); prefs.putUInt("cool_ms", cooldownMs); prefs.putUChar("stb_win", stableWinReq); prefs.end(); }

// ================= Sensor sampling =================
static inline int readADC(int pin){ return analogRead((uint8_t)pin); }

void sensorInit(){
  analogReadResolution(12); // 12-bit (0..4095)
  winStart = millis();
  accI = {0,0,0}; accV = {0,0,0};
}

void sensorLoop(){
  for(int i=0;i<3;i++){
    int ri = readADC(ACS712_PIN);
    int rv = readADC(ZMPT_PIN);
    int32_t di = (int32_t)ri - I_offset; // centered
    int32_t dv = (int32_t)rv - V_offset;
    accI.sumSq += (int64_t)di * (int64_t)di; accI.sum += di; accI.n++;
    accV.sumSq += (int64_t)dv * (int64_t)dv; accV.sum += dv; accV.n++;
  }
  uint32_t now = millis();
  if(now - winStart >= SENSE_WIN_MS){
    float i_counts = (accI.n>0)? sqrt((double)accI.sumSq / (double)accI.n) : 0.0;
    float v_counts = (accV.n>0)? sqrt((double)accV.sumSq / (double)accV.n) : 0.0;
    Irms = i_counts * I_scale;
    Vrms = v_counts * V_scale;

    // Auto-trigger window logic
    bool above = (Irms >= I_trig_A) && (Vrms >= V_cut_V);
    if(above) stableCnt++; else stableCnt = 0;

    // reset window accumulators
    accI = {0,0,0}; accV = {0,0,0};
    winStart = now;
  }
}

// ================= Guards & Auto =================
bool canTrigger(){
  if(!auto_en) return false;
  if(g_state != IDLE) return false;
  if(guard_en){
    if(Vrms < V_cut_V) return false; // undervoltage hold
  }
  // cooldown
  if(millis() - lastWeldEnd < cooldownMs) return false;
  // stability
  if(stableCnt < stableWinReq) return false;
  return true;
}

void abortWeld(const char* reason){
  digitalWrite(SSR_PIN, LOW);
  g_state = IDLE;
  addLog(String("ABORT: ")+reason);
}

// ================= API Routes =================
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

void handleCurrent(){ server.send(200, "application/json", String("{\"id\":"+String(g_selectedPreset)+"}")); }

void handleSelect(){ if(!server.hasArg("id")){ server.send(400, "text/plain", "missing id"); return;} int id = server.arg("id").toInt(); if(id<1||id>99){ server.send(400, "text/plain", "bad id"); return;} g_selectedPreset = (uint8_t)id; saveSelected(); Preset p; makePreset(g_selectedPreset,p); g_preMs = p.preMs; g_mainMs = p.mainMs; server.send(200, "text/plain", "OK"); }

void handleSensor(){
  String out = "{";
  out += "\"vrms\":" + String(Vrms, 3) + ",";
  out += "\"irms\":" + String(Irms, 3) + ",";
  out += "\"n\":" + String(accI.n + accV.n) + ",";
  out += "\"win_ms\":" + String((uint32_t)SENSE_WIN_MS);
  out += "}";
  server.send(200, "application/json", out);
}

void handleGuardLoad(){
  String out = "{";
  out += "\"auto_en\":" + String(auto_en?1:0) + ",";
  out += "\"guard_en\":" + String(guard_en?1:0) + ",";
  out += "\"i_trig\":" + String(I_trig_A,2) + ",";
  out += "\"v_cut\":" + String(V_cut_V,1) + ",";
  out += "\"i_lim\":" + String(I_lim_A,1) + ",";
  out += "\"cooldown_ms\":" + String(cooldownMs) + ",";
  out += "\"stable_win\":" + String(stableWinReq);
  out += "}";
  server.send(200, "application/json", out);
}

void handleGuardSave(){
  if(server.hasArg("auto")) auto_en = (server.arg("auto")=="1");
  if(server.hasArg("guard")) guard_en = (server.arg("guard")=="1");
  if(server.hasArg("i_trig")) I_trig_A = server.arg("i_trig").toFloat();
  if(server.hasArg("v_cut"))  V_cut_V  = server.arg("v_cut").toFloat();
  if(server.hasArg("i_lim"))  I_lim_A  = server.arg("i_lim").toFloat();
  if(server.hasArg("cd"))     cooldownMs = server.arg("cd").toInt();
  if(server.hasArg("sw"))     stableWinReq = (uint8_t)server.arg("sw").toInt();
  saveGuard();
  server.send(200, "text/plain", "OK");
}

void handleGuardStatus(){
  bool ready = canTrigger();
  String out = "{";
  out += "\"auto_en\":" + String(auto_en?1:0) + ",";
  out += "\"guard_en\":" + String(guard_en?1:0) + ",";
  out += "\"cooldown_ms\":" + String(cooldownMs) + ",";
  out += "\"stable_win\":" + String(stableWinReq) + ",";
  out += "\"ready\":" + String(ready?1:0) + ",";
  out += "\"last\":\"" + (logBuf[(logIdx+LOGN-1)%LOGN]) + "\"";
  out += "}";
  server.send(200, "application/json", out);
}

void handleLog(){
  String out = "[";
  for(int i=0;i<LOGN;i++){
    int idx = (logIdx + i) % LOGN;
    if(i>0) out += ",";
    out += "\"" + logBuf[idx] + "\"";
  }
  out += "]";
  server.send(200, "application/json", out);
}

// ================= Trigger & FSM =================
void startWeld(){
  if(g_state!=IDLE) return;
  g_state = (g_preMs>0? PRE_PULSE: MAIN_PULSE);
  g_t0 = millis();
  digitalWrite(SSR_PIN, (g_state!=IDLE));
  addLog("WELD START");
}

void fsmLoop(){
  unsigned long t = millis();
  // Guards during weld
  if(guard_en && g_state != IDLE){
    if(Irms > I_lim_A){ abortWeld("OverCurrent"); lastWeldEnd = millis(); return; }
    if(Vrms < V_cut_V){ abortWeld("UnderVoltage"); lastWeldEnd = millis(); return; }
  }

  switch(g_state){
    case IDLE:
      // Auto trigger decision in IDLE
      if(canTrigger()){
        stableCnt = 0; // consume stability
        startWeld();
      }
      break;
    case PRE_PULSE:
      if(t - g_t0 >= g_preMs){ digitalWrite(SSR_PIN, LOW); g_state = GAP; g_t0 = t; }
      break;
    case GAP:
      if(t - g_t0 >= g_gapMs){ g_state = MAIN_PULSE; g_t0 = t; digitalWrite(SSR_PIN, HIGH); }
      break;
    case MAIN_PULSE:
      if(t - g_t0 >= g_mainMs){ digitalWrite(SSR_PIN, LOW); g_state = IDLE; lastWeldEnd = millis(); addLog("WELD END"); }
      break;
  }
}

void handleTrigger(){ startWeld(); server.send(200, "text/plain", "Weld cycle started"); }

// ================= Setup / Loop =================
void setup(){
  Serial.begin(115200);
  pinMode(SSR_PIN, OUTPUT); digitalWrite(SSR_PIN, LOW);
  loadSelected();
  sensorInit();

  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(AP_SSID, AP_PASS, 1, 0, 4);
  Serial.println(ok ? "AP started: SpotWelder+" : "AP start failed!");
  Serial.print("AP IP: "); Serial.println(WiFi.softAPIP());
  Serial.printf("Build: %s\n", BUILD_VERSION);

  server.on("/", [](){ server.send(200, "text/html", applyVersion(indexHtml)); });
  server.on("/ui", [](){ server.send(200, "text/html", applyVersion(indexHtml)); });
  server.on("/trigger", handleTrigger);
  server.on("/version", [](){ server.send(200, "text/plain", BUILD_VERSION); });
  // Preset/API
  server.on("/api/presets", HTTP_GET, respondJsonPresets);
  server.on("/api/current", HTTP_GET, handleCurrent);
  server.on("/api/select", HTTP_GET, handleSelect);
  // Sensor/API
  server.on("/api/sensor", HTTP_GET, handleSensor);
  // Guard/API
  server.on("/api/guard/load", HTTP_GET, handleGuardLoad);
  server.on("/api/guard/save", HTTP_GET, handleGuardSave);
  server.on("/api/guard/status", HTTP_GET, handleGuardStatus);
  server.on("/api/log", HTTP_GET, handleLog);
  // OTA
  server.on("/update", HTTP_GET, [](){ if(!server.authenticate(OTA_USER, OTA_PASS)) return server.requestAuthentication(); server.send(200, "text/html", updateHtml); });
  server.on("/update", HTTP_POST, [](){ if(!server.authenticate(OTA_USER, OTA_PASS)) return server.requestAuthentication(); bool ok=!Update.hasError(); server.sendHeader("Connection","close"); server.send(200, "text/plain", ok?"OK":"FAIL"); delay(200); ESP.restart(); },
    [](){ if(!server.authenticate(OTA_USER, OTA_PASS)) return; HTTPUpload& u=server.upload(); if(u.status==UPLOAD_FILE_START){ if(!Update.begin(UPDATE_SIZE_UNKNOWN)){ Update.printError(Serial);} } else if(u.status==UPLOAD_FILE_WRITE){ if(Update.write(u.buf,u.currentSize)!=u.currentSize){ Update.printError(Serial);} } else if(u.status==UPLOAD_FILE_END){ if(Update.end(true)){ Serial.printf("[OTA] %u bytes\n", u.totalSize);} else { Update.printError(Serial);} } else if(u.status==UPLOAD_FILE_ABORTED){ Update.end(); Serial.println("[OTA] aborted"); } });
  // Aliases
  server.on("/update/", HTTP_GET, [](){ server.sendHeader("Location","/update"); server.send(302, "text/plain", ""); });
  server.on("/admin", HTTP_GET, [](){ server.sendHeader("Location","/update"); server.send(302, "text/plain", ""); });
  server.on("/ota", HTTP_GET, [](){ server.sendHeader("Location","/update"); server.send(302, "text/plain", ""); });
  server.onNotFound([](){ server.send(200, "text/html", applyVersion(indexHtml)); });
  server.begin();
}

void loop(){ server.handleClient(); sensorLoop(); fsmLoop(); }

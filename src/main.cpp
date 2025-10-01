#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>
#include <math.h>
#include "Config.h"

// ===== Globals =====
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

// ===== Preset model =====
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

// ===== HTML UI =====
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
.card{background:var(--card);padding:16px;border-radius:14px;max-width:980px;margin:0 auto}
.row{display:flex;gap:12px;flex-wrap:wrap}
.col{flex:1 1 300px}
.badge{display:inline-block;padding:4px 8px;border-radius:6px;background:#0f3460;}
small{opacity:.7}
hr{border:0;border-top:1px solid #2a3b63;margin:14px 0}
table{width:100%;border-collapse:collapse;margin-top:8px}
th,td{border-bottom:1px solid #2a3b63;padding:8px;text-align:left}
</style>
<script>
let presets=[]; let current=1; let grp=''; let q='';
async function load(){
  const pv = await fetch('/api/presets'); presets=await pv.json();
  const cv = await fetch('/api/current'); const cx = await cv.json(); current=cx.id;
  document.getElementById('ver').textContent='%v%';
  renderPresets();
  poll(); setInterval(poll, 600);
  loadCalib();
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
    document.getElementById('vrms').textContent = s.vrms.toFixed(1)+' V';
    document.getElementById('irms').textContent = s.irms.toFixed(2)+' A';
    document.getElementById('samples').textContent = s.n + ' samp / '+ s.win_ms +' ms';
  }catch(e){console.log(e)}
}

async function loadCalib(){
  const r = await fetch('/api/calib/load'); const c = await r.json();
  id('i_off').value = c.i_off; id('v_off').value = c.v_off; id('i_sc').value = c.i_sc.toFixed(6); id('v_sc').value = c.v_sc.toFixed(6);
}
async function zero(ch){ const r = await fetch('/api/calib/zero?ch='+ch); if(r.ok){ await loadCalib(); beep(); } }
async function save(){
  const i_sc = id('i_sc').value; const v_sc = id('v_sc').value;
  const url = `/api/calib/save?i_sc=${i_sc}&v_sc=${v_sc}`; const r = await fetch(url); if(r.ok){ beep(); }
}
async function autoV(){
  const target = prompt('Target Vrms (mis. 220):', '220'); if(!target) return;
  const r = await fetch('/api/calib/auto_v?target='+encodeURIComponent(target)); if(r.ok){ await loadCalib(); beep(); }
}
async function autoI(){
  const target = prompt('Target Irms (A):', '10'); if(!target) return;
  const r = await fetch('/api/calib/auto_i?target='+encodeURIComponent(target)); if(r.ok){ await loadCalib(); beep(); }
}

function id(s){return document.getElementById(s)}
function beep(){try{const c=new (window.AudioContext||window.webkitAudioContext)();const o=c.createOscillator();const g=c.createGain();o.type='square';o.frequency.value=720;g.gain.value=0.05;o.connect(g);g.connect(c.destination);o.start();setTimeout(()=>o.stop(),150);}catch(e){}}
async function trig(){ const r=await fetch('/trigger'); const t=await r.text(); beep(); document.getElementById('status').textContent=t; }
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
      </div>
      <div class="col">
        <p><b>Live Sensor</b></p>
        <div>Vrms: <b id="vrms">-</b></div>
        <div>Irms: <b id="irms">-</b></div>
        <small id="samples">-</small>
      </div>
    </div>
    <hr>
    <h3>Preset Table</h3>
    <table>
      <thead><tr><th>ID</th><th>Nama</th><th>Grup</th><th>Pre</th><th>Main</th><th></th></tr></thead>
      <tbody id="tbody"></tbody>
    </table>
    <hr>
    <h3>Calibration</h3>
    <div class="row">
      <div class="col">
        <p><b>Offsets (raw ADC center)</b></p>
        <div>Current offset: <input id="i_off" disabled style="width:120px"></div>
        <div>Voltage offset: <input id="v_off" disabled style="width:120px"></div>
        <p>
          <button class="btn" onclick="zero('i')">Zero Current</button>
          <button class="btn" onclick="zero('v')">Zero Voltage</button>
        </p>
      </div>
      <div class="col">
        <p><b>Scales</b></p>
        <div>I scale (A/count): <input id="i_sc" style="width:160px"></div>
        <div>V scale (V/count): <input id="v_sc" style="width:160px"></div>
        <p>
          <button class="btn" onclick="save()">Save</button>
          <button class="btn" onclick="autoV()">Auto-Scale Voltage (to target)</button>
          <button class="btn" onclick="autoI()">Auto-Scale Current (to target)</button>
        </p>
        <small>Catatan: lakukan <i>Zero</i> dulu tanpa beban, lalu atur scale dengan referensi meteran.</small>
      </div>
    </div>
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

// ===== Helpers =====
String applyVersion(const char* tpl){ String s(tpl); s.replace("%v%", BUILD_VERSION); return s; }

void loadSelected(){
  prefs.begin("swp", true);
  g_selectedPreset = prefs.getUChar("sel", 1);
  // calib
  I_offset = prefs.getInt("i_off", DEF_I_OFFSET);
  V_offset = prefs.getInt("v_off", DEF_V_OFFSET);
  I_scale  = prefs.getFloat("i_sc", DEF_I_SCALE);
  V_scale  = prefs.getFloat("v_sc", DEF_V_SCALE);
  prefs.end();
  // apply durations from preset
  Preset p; makePreset(g_selectedPreset, p);
  g_preMs = p.preMs; g_mainMs = p.mainMs; g_gapMs = 60;
}

void saveSelected(){ prefs.begin("swp", false); prefs.putUChar("sel", g_selectedPreset); prefs.end(); }

void saveCalib(){ prefs.begin("swp", false); prefs.putInt("i_off", I_offset); prefs.putInt("v_off", V_offset); prefs.putFloat("i_sc", I_scale); prefs.putFloat("v_sc", V_scale); prefs.end(); }

// ===== Sensor sampling =====
static inline int readADC(gpio_num_t pin){ return analogRead((uint8_t)pin); }

void sensorInit(){
  analogReadResolution(12); // 12-bit (0..4095)
  // Optional: set attenuation for 0..3.3V range (default is fine on Arduino core)
  winStart = millis();
  accI = {0,0,0}; accV = {0,0,0};
}

void sensorLoop(){
  // Take few samples per loop to avoid blocking; tune this if needed
  for(int i=0;i<3;i++){
    int ri = readADC((gpio_num_t)ACS712_PIN);
    int rv = readADC((gpio_num_t)ZMPT_PIN);
    int32_t di = (int32_t)ri - I_offset; // centered
    int32_t dv = (int32_t)rv - V_offset;
    accI.sumSq += (int64_t)di * (int64_t)di;
    accI.sum   += di;
    accI.n++;
    accV.sumSq += (int64_t)dv * (int64_t)dv;
    accV.sum   += dv;
    accV.n++;
  }
  uint32_t now = millis();
  if(now - winStart >= SENSE_WIN_MS){
    // Compute RMS from sumSq/n (counts), then scale
    float i_rms_counts = (accI.n>0)? sqrt((double)accI.sumSq / (double)accI.n) : 0.0;
    float v_rms_counts = (accV.n>0)? sqrt((double)accV.sumSq / (double)accV.n) : 0.0;
    Irms = i_rms_counts * I_scale;
    Vrms = v_rms_counts * V_scale;
    // reset window
    accI = {0,0,0}; accV = {0,0,0};
    winStart = now;
  }
}

// ===== Routes: Presets & Sensors =====
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

void handleCalibLoad(){
  String out = "{";
  out += "\"i_off\":" + String(I_offset) + ",";
  out += "\"v_off\":" + String(V_offset) + ",";
  out += "\"i_sc\":" + String(I_scale, 6) + ",";
  out += "\"v_sc\":" + String(V_scale, 6);
  out += "}";
  server.send(200, "application/json", out);
}

void handleCalibZero(){
  if(!server.hasArg("ch")){ server.send(400, "text/plain", "missing ch"); return; }
  String ch = server.arg("ch");
  // sample quick burst to estimate center
  const int N = 256;
  int64_t sum = 0;
  if(ch == "i"){
    for(int i=0;i<N;i++) sum += analogRead(ACS712_PIN);
    I_offset = (int)(sum / N);
  } else if(ch == "v"){
    for(int i=0;i<N;i++) sum += analogRead(ZMPT_PIN);
    V_offset = (int)(sum / N);
  } else { server.send(400, "text/plain", "bad ch"); return; }
  saveCalib();
  server.send(200, "text/plain", "OK");
}

void handleCalibSave(){
  if(server.hasArg("i_sc")) I_scale = server.arg("i_sc").toFloat();
  if(server.hasArg("v_sc")) V_scale = server.arg("v_sc").toFloat();
  saveCalib();
  server.send(200, "text/plain", "OK");
}

void handleAutoV(){
  if(!server.hasArg("target")){ server.send(400, "text/plain", "missing target"); return; }
  float target = server.arg("target").toFloat();
  // Use current window's RMS counts; prevent div by zero
  float counts = (V_scale>0.0f) ? (Vrms / V_scale) : 0.0f;
  if(counts < 1.0f){ server.send(400, "text/plain", "low signal"); return; }
  V_scale = target / counts;
  saveCalib();
  server.send(200, "text/plain", "OK");
}

void handleAutoI(){
  if(!server.hasArg("target")){ server.send(400, "text/plain", "missing target"); return; }
  float target = server.arg("target").toFloat();
  float counts = (I_scale>0.0f) ? (Irms / I_scale) : 0.0f;
  if(counts < 1.0f){ server.send(400, "text/plain", "low signal"); return; }
  I_scale = target / counts;
  saveCalib();
  server.send(200, "text/plain", "OK");
}

// ===== Trigger & FSM =====
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

// ===== Setup / Loop =====
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
  server.on("/api/calib/load", HTTP_GET, handleCalibLoad);
  server.on("/api/calib/zero", HTTP_GET, handleCalibZero);
  server.on("/api/calib/save", HTTP_GET, handleCalibSave);
  server.on("/api/calib/auto_v", HTTP_GET, handleAutoV);
  server.on("/api/calib/auto_i", HTTP_GET, handleAutoI);
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

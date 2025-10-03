#include <WiFi.h>
#include <WebServer.h>
#include <Update.h>
#include <Preferences.h>
#include <math.h>
#include "Config.h"

// ===== VERSION STRING PATCH (aman walau -D tanpa kutip) =====
#ifndef BUILD_VERSION
// fallback bila tidak didefinisikan dari platformio.ini
#define BUILD_VERSION Build_6_2_3_SpotPattern
#endif
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)
#define BUILD_VERSION_STR STR(BUILD_VERSION)
// ---- Forward declarations (route handlers) ----
void respondJsonPresets();
void handleCurrent();
void handleSelect();
void handleSensor();
void handleGuardLoad();
void handleGuardSave();
void handleGuardStatus();
void handleModeGet();
void handleModePost();
void handleWeldCfgGet();
void handleWeldCfgPost();
void smartOptions();
void smartConfigGet();
void smartConfigPost();
void smartStart();
void smartStop();
void smartApply();
void smartStatus();
void smartHistoryJson();
void smartHistoryCsv();
void handleTrigger();

// ================= Globals =================
WebServer server(80);
Preferences prefs; // NVS

// Operation mode (0=PRESET, 1=SMART)
uint8_t opMode = DEF_OP_MODE; // saved at /api/mode

// Spot Pattern
uint8_t spotPattern = DEF_SPOT_PATTERN; // 0=SINGLE,1=DOUBLE
uint16_t gapMs = DEF_GAP_MS;

// Preset selection
volatile uint8_t g_selectedPreset = 1; // 1..99

// Weld FSM
enum WeldState { IDLE, PRE_PULSE, GAP, MAIN_PULSE };
volatile WeldState g_state = IDLE;
unsigned long g_t0 = 0;
uint16_t g_preMs = 0, g_gapMs = DEF_GAP_MS, g_mainMs = 100; // defaults

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

// ================= Smart AI Structures =================
struct AiState {
  bool running = false;
  float t_mm = 0.10f;         // chosen thickness
  uint8_t trial = 0;          // 1..AI_MAX_TRIALS
  // baselines and tuned
  uint16_t base_pre = 30;     // ms
  uint16_t base_main = 80;    // ms
  uint16_t tuned_pre = 30;    // ms
  uint16_t tuned_main = 80;   // ms
  // references & band
  float irms_ref = 0.0f;      // from first trial
  float E_base = 0.0f, E_low = 0.0f, E_high = 0.0f;
  // last results
  String rating = "-";        // BAD/GOOD/BAD-HOT
  float E_est = 0.0f;
  float irms_main_avg = 0.0f;
  float dv_sag = 0.0f;        // Vrms_idle - Vrms_main
} ai;

// AI History
struct AiHistEntry { uint32_t ms; float t_mm; uint16_t pre_ms; uint16_t main_ms; float E_est; float irms; char rating[8]; };
AiHistEntry hist[AI_HIST_MAX];
uint8_t histCount=0, histHead=0; // ring buffer

void histPush(const AiHistEntry &e){ hist[histHead]=e; histHead=(histHead+1)%AI_HIST_MAX; if(histCount<AI_HIST_MAX) histCount++; }

// thickness list
static const float TH_LIST[] = {0.10f,0.12f,0.15f,0.18f,0.20f,0.25f};
static const int TH_COUNT = 6;

// baseline functions
static uint16_t base_pre_ms(float t){
  float v = t * 250.0f; if(v<AI_MIN_PRE_MS) v=AI_MIN_PRE_MS; if(v>AI_MAX_PRE_MS) v=AI_MAX_PRE_MS; return (uint16_t)(v+0.5f);
}
static float E_index(float t){
  if(t<=0.10f) return 1.00f; if(t<=0.12f) return 1.15f; if(t<=0.15f) return 1.35f; if(t<=0.18f) return 1.55f; if(t<=0.20f) return 1.70f; return 2.10f;
}
static uint16_t base_main_ms(float t){
  float v = 80.0f + 120.0f*(E_index(t)-1.0f); if(v<AI_MIN_MAIN_MS) v=AI_MIN_MAIN_MS; if(v>AI_MAX_MAIN_MS) v=AI_MAX_MAIN_MS; return (uint16_t)(v+0.5f);
}

// Load tuned durations for a thickness (fallback to baseline)
void smartLoadTuned(float tmm, uint16_t &pre, uint16_t &main){
  char kpre[24]; char kmain[24];
  snprintf(kpre, sizeof(kpre), "smart_%.2f_pre", tmm);
  snprintf(kmain, sizeof(kmain), "smart_%.2f_main", tmm);
  prefs.begin("swp", true);
  pre  = prefs.getUShort(kpre, base_pre_ms(tmm));
  main = prefs.getUShort(kmain, base_main_ms(tmm));
  prefs.end();
}

// Save tuned durations for a thickness
void smartSaveTuned(float tmm, uint16_t pre, uint16_t main){
  char kpre[24]; char kmain[24];
  snprintf(kpre, sizeof(kpre), "smart_%.2f_pre", tmm);
  snprintf(kmain, sizeof(kmain), "smart_%.2f_main", tmm);
  prefs.begin("swp", false);
  prefs.putUShort(kpre, pre);
  prefs.putUShort(kmain, main);
  prefs.end();
}

// ================= HTML UI =================
// Home page
static const char homeHtml[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>SpotWelder+ %v%</title>
<style>:root{--bg:#0b132b;--card:#1b2a49;--fg:#e0e0e0;--btn:#1c2541;--accent:#5bc0be}*{box-sizing:border-box}
body{font-family:system-ui,Segoe UI,Roboto,Arial;margin:20px;background:var(--bg);color:var(--fg)}
.btn{font-size:16px;padding:10px 14px;border-radius:10px;border:none;background:var(--btn);color:#fff;cursor:pointer;text-decoration:none;display:inline-block}
.card{background:var(--card);padding:16px;border-radius:14px;max-width:1040px;margin:0 auto}
.row{display:flex;gap:12px;flex-wrap:wrap}.col{flex:1 1 320px}.badge{display:inline-block;padding:4px 8px;border-radius:6px;background:#0f3460}
small{opacity:.7}input,select{padding:8px;border-radius:8px;border:1px solid #2a3b63;background:#0f1b3d;color:#fff}
</style>
<script>
async function load(){ id('ver').textContent='%v%'; await loadMode(); await loadWcfg(); poll(); setInterval(poll, 600); }
async function poll(){ try{ const s=await (await fetch('/api/sensor')).json(); id('vrms').textContent=s.vrms.toFixed(1)+' V'; id('irms').textContent=s.irms.toFixed(2)+' A'; const g=await (await fetch('/api/guard/status')).json(); id('ready').textContent=g.ready?'READY':'HOLD'; }catch(e){} }
async function loadMode(){ const m=await (await fetch('/api/mode')).json(); id('mode').textContent=m.mode; id('modeSel').value=m.mode; }
async function setMode(){ const v=id('modeSel').value; await fetch('/api/mode?m='+v,{method:'POST'}); await loadMode(); beep(); }
async function loadWcfg(){ const r=await (await fetch('/api/weldcfg')).json(); id('pattern').value=r.pattern; id('gap').value=r.gap_ms; id('pat_label').textContent=r.pattern; id('gap_label').textContent=r.gap_ms+' ms'; }
async function saveWcfg(){ const p=new URLSearchParams({pattern:id('pattern').value,gap_ms:id('gap').value}); await fetch('/api/weldcfg?'+p.toString(),{method:'POST'}); await loadWcfg(); beep(); }
function id(s){return document.getElementById(s)}
function beep(){try{const c=new (window.AudioContext||window.webkitAudioContext)();const o=c.createOscillator();const g=c.createGain();o.type='square';o.frequency.value=720;g.gain.value=0.05;o.connect(g);g.connect(c.destination);o.start();setTimeout(()=>o.stop(),140);}catch(e){}}
async function trig(){ const r=await fetch('/trigger'); const t=await r.text(); beep(); id('status').textContent=t; }
</script></head>
<body onload="load()">
<div class="card">
  <h2>SpotWelder+ <span class="badge" id="ver"></span></h2>
  <div class="row">
    <div class="col">
      <p><b>Trigger</b> (<span id="mode">-</span> mode)</p>
      <button class="btn" onclick="trig()">Trigger Weld</button>
      <p id="status"><small>Ready.</small></p>
      <hr>
      <p><b>Operation Mode</b>: <select id="modeSel" onchange="setMode()"><option>PRESET</option><option>SMART</option></select></p>
      <p>Smart AI Spot: <a class="btn" href="/smart">Open Smart Page</a></p>
      <p>OTA Update: <a class="btn" href="/update">Open OTA Updater</a></p>
    </div>
    <div class="col">
      <p><b>Live Sensor</b></p>
      <div>Vrms: <b id="vrms">-</b></div>
      <div>Irms: <b id="irms">-</b></div>
      <small>Status: <b id="ready">-</b></small>
    </div>
    <div class="col">
      <p><b>Spot Pattern</b></p>
      <div>Pattern: <select id="pattern"><option value="SINGLE">SINGLE</option><option value="DOUBLE">DOUBLE</option></select></div>
      <div style="margin-top:6px">Gap (ms): <input id="gap" style="width:120px" type="number" min="10" max="200" step="5"></div>
      <p style="margin-top:8px"><button class="btn" onclick="saveWcfg()">Save Pattern</button></p>
      <small>Now: <b id="pat_label">-</b>, gap=<b id="gap_label">-</b></small>
    </div>
  </div>
</div>
</body></html>
)rawliteral";

// Smart page (shows pattern/gap and allows editing too)
static const char smartHtml[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>Smart AI Spot — %v%</title>
<style>:root{--bg:#0b132b;--card:#1b2a49;--fg:#e0e0e0;--btn:#1c2541;--accent:#5bc0be}*{box-sizing:border-box}
body{font-family:system-ui,Segoe UI,Roboto,Arial;margin:20px;background:var(--bg);color:var(--fg)}
.btn{font-size:16px;padding:10px 14px;border-radius:10px;border:none;background:var(--btn);color:#fff;cursor:pointer;text-decoration:none;display:inline-block}
.card{background:var(--card);padding:16px;border-radius:14px;max-width:1100px;margin:0 auto}
.row{display:flex;gap:12px;flex-wrap:wrap}.col{flex:1 1 340px}.badge{display:inline-block;padding:4px 8px;border-radius:6px;background:#0f3460}
small{opacity:.7}table{width:100%;border-collapse:collapse;margin-top:8px}th,td{border-bottom:1px solid #2a3b63;padding:8px;text-align:left}
input,select{padding:8px;border-radius:8px;border:1px solid #2a3b63;background:#0f1b3d;color:#fff}
</style>
<script>
async function load(){ id('ver').textContent='%v%'; await smartLoad(); await loadWcfg(); poll(); setInterval(poll, 600); }
async function poll(){ try{ const st=await (await fetch('/smart/status')).json(); id('ai_state').textContent=st.running?'RUNNING':'IDLE'; id('ai_th').textContent=st.t_mm.toFixed(2)+' mm'; id('ai_rating').textContent=st.rating; id('ai_trial').textContent=st.trial; id('ai_pre').textContent=st.tuned_pre+' ms'; id('ai_main').textContent=st.tuned_main+' ms'; id('ai_E').textContent=st.E_est.toFixed(1); id('ai_irms').textContent=st.irms_main_avg.toFixed(2)+' A'; }catch(e){} }
async function smartLoad(){ const o=await (await fetch('/smart/options')).json(); const sel=id('th'); sel.innerHTML=''; for(const t of o.thickness){ const opt=document.createElement('option'); opt.value=t; opt.text=t.toFixed(2)+' mm'; sel.appendChild(opt);} const c=await (await fetch('/smart/config')).json(); sel.value=c.t_mm; id('base_pre').textContent=c.base_pre+' ms'; id('base_main').textContent=c.base_main+' ms'; id('tuned_pre').textContent=c.tuned_pre+' ms'; id('tuned_main').textContent=c.tuned_main+' ms'; }
async function smartSet(){ const t=id('th').value; await fetch('/smart/config',{method:'POST',body:new URLSearchParams({t_mm:t})}); await smartLoad(); beep(); }
async function smartStart(){ await fetch('/smart/start',{method:'POST'}); beep(); }
async function smartStop(){ await fetch('/smart/stop',{method:'POST'}); beep(); }
async function smartApply(){ await fetch('/smart/apply',{method:'POST'}); beep(); }
async function loadWcfg(){ const r=await (await fetch('/api/weldcfg')).json(); id('pattern').value=r.pattern; id('gap').value=r.gap_ms; id('pat_label').textContent=r.pattern; id('gap_label').textContent=r.gap_ms+' ms'; }
async function saveWcfg(){ const p=new URLSearchParams({pattern:id('pattern').value,gap_ms:id('gap').value}); await fetch('/api/weldcfg?'+p.toString(),{method:'POST'}); await loadWcfg(); beep(); }
function id(s){return document.getElementById(s)}
function beep(){try{const c=new (window.AudioContext||window.webkitAudioContext)();const o=c.createOscillator();const g=c.createGain();o.type='square';o.frequency.value=720;g.gain.value=0.05;o.connect(g);g.connect(c.destination);o.start();setTimeout(()=>o.stop(),140);}catch(e){}}
</script>
</head>
<body onload="load()">
<div class="card">
  <h2>Smart AI Spot <span class="badge" id="ver"></span></h2>
  <p><a class="btn" href="/">◀ Back to Home</a> <a class="btn" href="/smart/history.csv">Download History CSV</a></p>
  <div class="row">
    <div class="col">
      <p><b>Material</b>: Nickel</p>
      <p><b>Thickness</b>: <select id="th" onchange="smartSet()"></select></p>
      <p>Baseline: pre=<b id="base_pre">-</b>, main=<b id="base_main">-</b></p>
      <p>Tuned (saved): pre=<b id="tuned_pre">-</b>, main=<b id="tuned_main">-</b></p>
      <p>
        <button class="btn" onclick="smartStart()">Start Auto‑Tune</button>
        <button class="btn" onclick="smartStop()">Stop</button>
        <button class="btn" onclick="smartApply()">Apply Tuned</button>
      </p>
    </div>
    <div class="col">
      <p><b>Status</b>: <b id="ai_state">-</b> (trial <b id="ai_trial">-</b>)</p>
      <div>pre=<b id="ai_pre">-</b>, main=<b id="ai_main">-</b></div>
      <div>E_est=<b id="ai_E">-</b>, Irms(main)=<b id="ai_irms">-</b></div>
      <div>t=<b id="ai_th">-</b></div>
    </div>
    <div class="col">
      <p><b>Spot Pattern</b> (Smart mode juga memakai pattern & gap ini)</p>
      <div>Pattern: <select id="pattern"><option value="SINGLE">SINGLE</option><option value="DOUBLE">DOUBLE</option></select></div>
      <div style="margin-top:6px">Gap (ms): <input id="gap" style="width:120px" type="number" min="10" max="200" step="5"></div>
      <p style="margin-top:8px"><button class="btn" onclick="saveWcfg()">Save Pattern</button></p>
      <small>Now: <b id="pat_label">-</b>, gap=<b id="gap_label">-</b></small>
    </div>
  </div>
  <hr>
  <h3>History (latest first)</h3>
  <table>
    <thead><tr><th>ms</th><th>t(mm)</th><th>pre</th><th>main</th><th>E_est</th><th>Irms</th><th>rating</th></tr></thead>
    <tbody id="hist"></tbody>
  </table>
</div>
</body></html>
)rawliteral";

static const char updateHtml[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width, initial-scale=1"/>
<title>OTA Update</title>
<style>body{font-family:system-ui,Segoe UI,Roboto,Arial;margin:24px;background:#0b132b;color:#e0e0e0}.card{background:#1b2a49;padding:20px;border-radius:14px;max-width:520px}input[type=file]{background:#0f3460;padding:10px;border-radius:8px}button{font-size:16px;padding:10px 16px;border-radius:8px;border:none;background:#1c2541;color:#fff;cursor:pointer}</style>
</head><body>
  <h2>OTA Update</h2>
  <div class="card">
    <form method="POST" action="/update" enctype="multipart/form-data">
      <p>Pilih file firmware (<code>.bin</code>) lalu klik <b>Upload</b>.</p>
      <input type="file" name="firmware" accept=".bin" required>
      <button type="submit">Upload</button>
    </form>
    <p><small>Gunakan file: <code>spotweldingplus-app.bin</code> (hasil CI/Release)</small></p>
  </div>
</body></html>
)rawliteral";

// ================= Helpers =================
String applyVersion(const char* tpl){ String s(tpl); s.replace("%v%", BUILD_VERSION); return s; }

void loadAll(){
  prefs.begin("swp", true);
  g_selectedPreset = prefs.getUChar("sel", 1);
  I_offset = prefs.getInt("i_off", DEF_I_OFFSET);
  V_offset = prefs.getInt("v_off", DEF_V_OFFSET);
  I_scale  = prefs.getFloat("i_sc", DEF_I_SCALE);
  V_scale  = prefs.getFloat("v_sc", DEF_V_SCALE);
  auto_en  = prefs.getBool("auto_en", DEF_AUTO_EN);
  guard_en = prefs.getBool("grd_en", DEF_GUARD_EN);
  I_trig_A = prefs.getFloat("i_trig", DEF_I_TRIG_A);
  V_cut_V  = prefs.getFloat("v_cut", DEF_V_CUTOFF_V);
  I_lim_A  = prefs.getFloat("i_lim", DEF_I_LIMIT_A);
  cooldownMs = prefs.getUInt("cool_ms", DEF_COOLDOWN_MS);
  stableWinReq = (uint8_t)prefs.getUChar("stb_win", DEF_STABLE_WIN);
  opMode = (uint8_t)prefs.getUChar("op_mode", DEF_OP_MODE);
  ai.t_mm = prefs.getFloat("smart_t", 0.10f);
  // Spot Pattern
  spotPattern = (uint8_t)prefs.getUChar("w_pat", DEF_SPOT_PATTERN);
  gapMs = prefs.getUShort("w_gap", DEF_GAP_MS);
  prefs.end();
}

void saveMode(){ prefs.begin("swp", false); prefs.putUChar("op_mode", opMode); prefs.end(); }
void saveSmartT(){ prefs.begin("swp", false); prefs.putFloat("smart_t", ai.t_mm); prefs.end(); }
void saveWeldCfg(){ prefs.begin("swp", false); prefs.putUChar("w_pat", spotPattern); prefs.putUShort("w_gap", gapMs); prefs.end(); }

static inline int readADC(int pin){ return analogRead((uint8_t)pin); }

void sensorInit(){ analogReadResolution(12); winStart = millis(); accI = {0,0,0}; accV = {0,0,0}; }

void sensorLoop(){
  for(int i=0;i<3;i++){
    int ri = readADC(ACS712_PIN);
    int rv = readADC(ZMPT_PIN);
    int32_t di = (int32_t)ri - I_offset; int32_t dv = (int32_t)rv - V_offset;
    accI.sumSq += (int64_t)di * (int64_t)di; accI.sum += di; accI.n++;
    accV.sumSq += (int64_t)dv * (int64_t)dv; accV.sum += dv; accV.n++;
  }
  uint32_t now = millis();
  if(now - winStart >= SENSE_WIN_MS){
    float i_counts = (accI.n>0)? sqrt((double)accI.sumSq/(double)accI.n) : 0.0;
    float v_counts = (accV.n>0)? sqrt((double)accV.sumSq/(double)accV.n) : 0.0;
    Irms = i_counts * I_scale; Vrms = v_counts * V_scale;
    bool above = (Irms >= I_trig_A) && (Vrms >= V_cut_V);
    if(above) stableCnt++; else stableCnt = 0;
    accI = {0,0,0}; accV = {0,0,0}; winStart = now;
  }
}

bool canTrigger(){ if(!auto_en) return false; if(g_state != IDLE) return false; if(guard_en && Vrms < V_cut_V) return false; if(millis() - lastWeldEnd < cooldownMs) return false; if(stableCnt < stableWinReq) return false; return true; }
void abortWeld(const char* reason){ digitalWrite(SSR_PIN, LOW); g_state = IDLE; addLog(String("ABORT: ")+reason); }

// Smart helpers
void aiComputeFromT(){ ai.base_pre = base_pre_ms(ai.t_mm); ai.base_main = base_main_ms(ai.t_mm); smartLoadTuned(ai.t_mm, ai.tuned_pre, ai.tuned_main); }
void aiStart(){ ai.running = true; ai.trial = 0; ai.rating = "-"; ai.E_est = 0; ai.irms_main_avg = 0; ai.dv_sag = 0; aiComputeFromT(); }
void aiStop(){ ai.running = false; }

void aiPostCycle(){
  float i_main = Irms; // simplified main average
  ai.irms_main_avg = i_main;
  if(ai.trial==1){ ai.irms_ref = (i_main>0? i_main: 10.0f); float Eb = ai.irms_ref*(ai.base_pre+ai.base_main); ai.E_base=Eb; ai.E_low=AI_BAND_LOW*Eb; ai.E_high=AI_BAND_HIGH*Eb; }
  ai.E_est = i_main * (ai.tuned_pre + ai.tuned_main);
  bool aborted=false; String last=logBuf[(logIdx+LOGN-1)%LOGN]; if(last.startsWith("ABORT:")) aborted=true;
  if(aborted || ai.E_est > ai.E_high){ ai.rating = "BAD-HOT"; ai.tuned_main = (uint16_t) fmaxf(AI_MIN_MAIN_MS, (float)ai.tuned_main*(1.0f-AI_DOWN_MAIN_PCT)); }
  else if(ai.E_est < ai.E_low){ ai.rating = "BAD"; ai.tuned_main = (uint16_t) fminf(AI_MAX_MAIN_MS, (float)ai.tuned_main*(1.0f+AI_UP_MAIN_PCT)); ai.tuned_pre  = (uint16_t) fminf(AI_MAX_PRE_MS,  (float)ai.tuned_pre*(1.0f+AI_UP_PRE_PCT)); }
  else { ai.rating = "GOOD"; float center=0.5f*(ai.E_low+ai.E_high); if(ai.E_est<center) ai.tuned_main=(uint16_t) fminf(AI_MAX_MAIN_MS,(float)ai.tuned_main*(1.0f+AI_FINE_PCT)); else ai.tuned_main=(uint16_t) fmaxf(AI_MIN_MAIN_MS,(float)ai.tuned_main*(1.0f-AI_FINE_PCT)); }
  if(ai.tuned_pre<AI_MIN_PRE_MS) ai.tuned_pre=AI_MIN_PRE_MS; if(ai.tuned_pre>AI_MAX_PRE_MS) ai.tuned_pre=AI_MAX_PRE_MS;
  if(ai.tuned_main<AI_MIN_MAIN_MS) ai.tuned_main=AI_MIN_MAIN_MS; if(ai.tuned_main>AI_MAX_MAIN_MS) ai.tuned_main=AI_MAX_MAIN_MS;
  // push history
  AiHistEntry e; e.ms=millis(); e.t_mm=ai.t_mm; e.pre_ms=ai.tuned_pre; e.main_ms=ai.tuned_main; e.E_est=ai.E_est; e.irms=ai.irms_main_avg; strncpy(e.rating, ai.rating.c_str(), sizeof(e.rating)); e.rating[sizeof(e.rating)-1]='\0';
  histPush(e);
}

// Apply op mode + pattern
void applyModeDurations(){
  // Base durations from selected mode
  uint16_t pre=0, main=0;
  if(opMode==MODE_SMART){ smartLoadTuned(ai.t_mm, pre, main); }
  else { Preset p; makePreset(g_selectedPreset,p); pre=p.preMs; main=p.mainMs; }
  // Apply pattern
  if(spotPattern==PATTERN_SINGLE){ pre = 0; }
  g_preMs = pre; g_mainMs = main; g_gapMs = gapMs; // gap used only if pre>0
}

// ================= API Routes =================
void respondJsonPresets(){ String out="["; for(uint8_t i=1;i<=PRESET_COUNT;i++){ Preset p; makePreset(i,p); if(i>1) out+=","; out+="{"; out+="\"id\":"+String(p.id)+","; out+="\"name\":\""+String(p.name)+"\","; out+="\"group\":\""+String(p.group)+"\","; out+="\"preMs\":"+String(p.preMs)+","; out+="\"mainMs\":"+String(p.mainMs); out+="}"; } out+="]"; server.send(200,"application/json",out); }
void handleCurrent(){ server.send(200,"application/json", String("{\"id\":"+String(g_selectedPreset)+"}")); }
void handleSelect(){ if(!server.hasArg("id")){ server.send(400,"text/plain","missing id"); return;} int id=server.arg("id").toInt(); if(id<1||id>99){ server.send(400,"text/plain","bad id"); return;} g_selectedPreset=(uint8_t)id; prefs.begin("swp",false); prefs.putUChar("sel",g_selectedPreset); prefs.end(); Preset p; makePreset(g_selectedPreset,p); g_preMs=p.preMs; g_mainMs=p.mainMs; server.send(200,"text/plain","OK"); }

void handleSensor(){ String out="{"; out+="\"vrms\":"+String(Vrms,3)+","; out+="\"irms\":"+String(Irms,3)+","; out+="\"n\":"+String(accI.n+accV.n)+","; out+="\"win_ms\":"+String((uint32_t)SENSE_WIN_MS); out+="}"; server.send(200,"application/json",out); }

void handleGuardLoad(){ String out="{"; out+="\"auto_en\":"+String(auto_en?1:0)+","; out+="\"guard_en\":"+String(guard_en?1:0)+","; out+="\"i_trig\":"+String(I_trig_A,2)+","; out+="\"v_cut\":"+String(V_cut_V,1)+","; out+="\"i_lim\":"+String(I_lim_A,1)+","; out+="\"cooldown_ms\":"+String(cooldownMs)+","; out+="\"stable_win\":"+String(stableWinReq); out+="}"; server.send(200,"application/json",out); }
void handleGuardSave(){ if(server.hasArg("auto")) auto_en=(server.arg("auto")=="1"); if(server.hasArg("guard")) guard_en=(server.arg("guard")=="1"); if(server.hasArg("i_trig")) I_trig_A=server.arg("i_trig").toFloat(); if(server.hasArg("v_cut")) V_cut_V=server.arg("v_cut").toFloat(); if(server.hasArg("i_lim")) I_lim_A=server.arg("i_lim").toFloat(); if(server.hasArg("cd")) cooldownMs=server.arg("cd").toInt(); if(server.hasArg("sw")) stableWinReq=(uint8_t)server.arg("sw").toInt(); prefs.begin("swp",false); prefs.putBool("auto_en",auto_en); prefs.putBool("grd_en",guard_en); prefs.putFloat("i_trig",I_trig_A); prefs.putFloat("v_cut",V_cut_V); prefs.putFloat("i_lim",I_lim_A); prefs.putUInt("cool_ms",cooldownMs); prefs.putUChar("stb_win",stableWinReq); prefs.end(); server.send(200,"text/plain","OK"); }
void handleGuardStatus(){ bool ready=canTrigger(); String out="{"; out+="\"auto_en\":"+String(auto_en?1:0)+","; out+="\"guard_en\":"+String(guard_en?1:0)+","; out+="\"cooldown_ms\":"+String(cooldownMs)+","; out+="\"stable_win\":"+String(stableWinReq)+","; out+="\"ready\":"+String(ready?1:0)+","; out+="\"last\":\""+(logBuf[(logIdx+LOGN-1)%LOGN])+"\""; out+="}"; server.send(200,"application/json",out); }

// Mode API
void handleModeGet(){ server.send(200,"application/json", String("{\"mode\":\""+(opMode==MODE_SMART?String("SMART"):String("PRESET"))+"\"}")); }
void handleModePost(){ if(!server.hasArg("m")){ server.send(400,"text/plain","missing m"); return;} String m=server.arg("m"); if(m=="SMART") opMode=MODE_SMART; else opMode=MODE_PRESET; saveMode(); server.send(200,"text/plain","OK"); }

// Weld Config API
void handleWeldCfgGet(){ String pat = (spotPattern==PATTERN_DOUBLE?"DOUBLE":"SINGLE"); String out="{"; out+="\"pattern\":\""+pat+"\","; out+="\"gap_ms\":"+String((int)gapMs); out+="}"; server.send(200,"application/json",out); }
void handleWeldCfgPost(){ if(server.hasArg("pattern")){ String p=server.arg("pattern"); spotPattern = (p=="DOUBLE"? PATTERN_DOUBLE: PATTERN_SINGLE); } if(server.hasArg("gap_ms")){ int v=server.arg("gap_ms").toInt(); if(v<10) v=10; if(v>200) v=200; gapMs=(uint16_t)v; } saveWeldCfg(); server.send(200,"text/plain","OK"); }

// Smart API
void smartOptions(){ String out="{\"thickness\":["; for(int i=0;i<TH_COUNT;i++){ if(i) out+=","; out+=String(TH_LIST[i],2);} out+="]}"; server.send(200,"application/json",out); }
void smartConfigGet(){ aiComputeFromT(); String out="{"; out+="\"t_mm\":"+String(ai.t_mm,2)+","; out+="\"base_pre\":"+String(ai.base_pre)+","; out+="\"base_main\":"+String(ai.base_main)+","; out+="\"tuned_pre\":"+String(ai.tuned_pre)+","; out+="\"tuned_main\":"+String(ai.tuned_main); out+="}"; server.send(200,"application/json",out); }
void smartConfigPost(){ if(server.hasArg("t_mm")) ai.t_mm=server.arg("t_mm").toFloat(); saveSmartT(); aiComputeFromT(); server.send(200,"text/plain","OK"); }
void smartStart(){ aiStart(); server.send(200,"text/plain","OK"); }
void smartStop(){ aiStop(); server.send(200,"text/plain","OK"); }
void smartApply(){ smartSaveTuned(ai.t_mm, ai.tuned_pre, ai.tuned_main); if(opMode==MODE_SMART){ applyModeDurations(); } server.send(200,"text/plain","OK"); }
void smartStatus(){ String out="{"; out+="\"running\":"+String(ai.running?1:0)+","; out+="\"t_mm\":"+String(ai.t_mm,2)+","; out+="\"trial\":"+String(ai.trial)+","; out+="\"rating\":\""+ai.rating+"\","; out+="\"tuned_pre\":"+String(ai.tuned_pre)+","; out+="\"tuned_main\":"+String(ai.tuned_main)+","; out+="\"E_est\":"+String(ai.E_est,1)+","; out+="\"irms_main_avg\":"+String(ai.irms_main_avg,2); out+="}"; server.send(200,"application/json",out); }

// History JSON (latest first)
void smartHistoryJson(){ String out="["; int n=histCount; for(int i=0;i<n;i++){ int idx=(histHead - 1 - i + AI_HIST_MAX)%AI_HIST_MAX; const AiHistEntry &e=hist[idx]; if(i) out+=","; out+="{"; out+="\"ms\":"+String(e.ms)+","; out+="\"t_mm\":"+String(e.t_mm,2)+","; out+="\"pre_ms\":"+String(e.pre_ms)+","; out+="\"main_ms\":"+String(e.main_ms)+","; out+="\"E_est\":"+String(e.E_est,1)+","; out+="\"irms\":"+String(e.irms,2)+","; out+="\"rating\":\""+String(e.rating)+"\""; out+="}"; } out+="]"; server.send(200,"application/json",out); }

// History CSV
void smartHistoryCsv(){ String out="ms,t_mm,pre_ms,main_ms,E_est,irms,rating\n"; int n=histCount; for(int i=0;i<n;i++){ int idx=(histHead - 1 - i + AI_HIST_MAX)%AI_HIST_MAX; const AiHistEntry &e=hist[idx]; out+=String(e.ms)+","+String(e.t_mm,2)+","+String(e.pre_ms)+","+String(e.main_ms)+","+String(e.E_est,1)+","+String(e.irms,2)+","+String(e.rating)+"\n"; } server.send(200,"text/csv",out); }

// ================= Trigger & FSM =================
void startWeld(){ if(g_state!=IDLE) return; g_state=(g_preMs>0? PRE_PULSE: MAIN_PULSE); g_t0=millis(); digitalWrite(SSR_PIN,(g_state!=IDLE)); addLog("WELD START"); }

void fsmLoop(){ unsigned long t=millis(); if(guard_en && g_state!=IDLE){ if(Irms>I_lim_A){ abortWeld("OverCurrent"); lastWeldEnd=millis(); return; } if(Vrms<V_cut_V){ abortWeld("UnderVoltage"); lastWeldEnd=millis(); return; } }
  switch(g_state){
    case IDLE: applyModeDurations(); if(canTrigger()){ stableCnt=0; startWeld(); } break;
    case PRE_PULSE: if(t-g_t0>=g_preMs){ digitalWrite(SSR_PIN,LOW); g_state=GAP; g_t0=t; } break;
    case GAP: if(t-g_t0>=g_gapMs){ g_state=MAIN_PULSE; g_t0=t; digitalWrite(SSR_PIN,HIGH); } break;
    case MAIN_PULSE: if(t-g_t0>=g_mainMs){ digitalWrite(SSR_PIN,LOW); g_state=IDLE; lastWeldEnd=millis(); addLog("WELD END"); if(ai.running){ ai.trial++; aiPostCycle(); if(ai.rating=="GOOD" || ai.trial>=AI_MAX_TRIALS){ smartSaveTuned(ai.t_mm, ai.tuned_pre, ai.tuned_main); ai.running=false; } } } break;
  }
}

void handleTrigger(){ startWeld(); server.send(200,"text/plain","Weld cycle started"); }

// ================= Setup / Loop =================
void setup(){ Serial.begin(115200); pinMode(SSR_PIN,OUTPUT); digitalWrite(SSR_PIN,LOW); loadAll(); sensorInit(); aiComputeFromT();
  WiFi.mode(WIFI_AP); bool ok=WiFi.softAP(AP_SSID,AP_PASS,1,0,4); Serial.println(ok?"AP started: SpotWelder+":"AP start failed!"); Serial.print("AP IP: "); Serial.println(WiFi.softAPIP()); Serial.printf("Build: %s\n", BUILD_VERSION);
  server.on("/", [](){ server.send(200,"text/html", applyVersion(homeHtml)); });
  server.on("/ui", [](){ server.send(200,"text/html", applyVersion(homeHtml)); });
  server.on("/smart", [](){ server.send(200,"text/html", applyVersion(smartHtml)); });
  server.on("/smart/", [](){ server.sendHeader("Location","/smart"); server.send(302,"text/plain",""); });
  server.on("/trigger", handleTrigger);
  server.on("/version", [](){ server.send(200,"text/plain", BUILD_VERSION); });
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
  // Mode/API
  server.on("/api/mode", HTTP_GET, handleModeGet);
  server.on("/api/mode", HTTP_POST, handleModePost);
  // WeldCfg/API
  server.on("/api/weldcfg", HTTP_GET, handleWeldCfgGet);
  server.on("/api/weldcfg", HTTP_POST, handleWeldCfgPost);
  // Smart/API
  server.on("/smart/options", HTTP_GET, smartOptions);
  server.on("/smart/config", HTTP_GET, smartConfigGet);
  server.on("/smart/config", HTTP_POST, smartConfigPost);
  server.on("/smart/start", HTTP_POST, smartStart);
  server.on("/smart/stop",  HTTP_POST, smartStop);
  server.on("/smart/apply", HTTP_POST, smartApply);
  server.on("/smart/status", HTTP_GET, smartStatus);
  server.on("/smart/history", HTTP_GET, smartHistoryJson);
  server.on("/smart/history.csv", HTTP_GET, smartHistoryCsv);
  
             // OTA
             extern "C" void initRoutes623();
initRoutes623();  // daftarkan route 6.2.3 (version.json + kalibrasi)
  server.on("/update", HTTP_GET, [](){ if(!server.authenticate(OTA_USER, OTA_PASS)) return server.requestAuthentication(); server.send(200,"text/html", updateHtml); });
  server.on("/update", HTTP_POST, [](){ if(!server.authenticate(OTA_USER, OTA_PASS)) return server.requestAuthentication(); bool ok=!Update.hasError(); server.sendHeader("Connection","close"); server.send(200,"text/plain", ok?"OK":"FAIL"); delay(200); ESP.restart(); },
    [](){ if(!server.authenticate(OTA_USER, OTA_PASS)) return; HTTPUpload& u=server.upload(); if(u.status==UPLOAD_FILE_START){ if(!Update.begin(UPDATE_SIZE_UNKNOWN)){ Update.printError(Serial);} } else if(u.status==UPLOAD_FILE_WRITE){ if(Update.write(u.buf,u.currentSize)!=u.currentSize){ Update.printError(Serial);} } else if(u.status==UPLOAD_FILE_END){ if(Update.end(true)){ Serial.printf("[OTA] %u bytes\n", u.totalSize);} else { Update.printError(Serial);} } else if(u.status==UPLOAD_FILE_ABORTED){ Update.end(); Serial.println("[OTA] aborted"); } });
  // Aliases & 404
  server.on("/update/", HTTP_GET, [](){ server.sendHeader("Location","/update"); server.send(302,"text/plain",""); });
  server.on("/admin", HTTP_GET, [](){ server.sendHeader("Location","/update"); server.send(302,"text/plain",""); });
  server.on("/ota", HTTP_GET, [](){ server.sendHeader("Location","/update"); server.send(302,"text/plain",""); });
  server.onNotFound([](){ server.send(200,"text/html", applyVersion(homeHtml)); });
  server.begin(); }

void loop(){ server.handleClient(); sensorLoop(); fsmLoop(); }

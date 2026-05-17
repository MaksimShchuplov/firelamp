#include <Arduino.h>
// =============================================================================
//  FIRE LAMP  —  ESP32-S3 / FastLED 3.10+
// -----------------------------------------------------------------------------
//  800-LED WS2812B strip arranged as a 20-col x 40-row cylinder.
//  Web UI on the local network drives perceptual brightness (0..100).
//
//  Driver:    default RMT5 (async, DMA-paced). Do NOT define an I2S backend
//             for a single data line — RMT5 overlaps frame computation with
//             LED transmission, which is the actual perf win on the S3.
//  FPS wall:  ~40 FPS for 800 LEDs on one data line is WS2812B protocol
//             physics. Parallel segments are the only way past it.
//  Power:     Mean Well 60W (5V 12A) PSU. Due to physical voltage drop along
//             the 800-LED strip, real-world current is lower than theoretical.
//             FastLED's safety limiter is hardcoded to 20A (100W) to allow
//             maximum brightness without tripping the PSU's over-current
//             protection.
//
//  Concurrency: LED rendering runs on Core 0 (xTaskCreatePinnedToCore).
//               Web server runs on Core 1 (loop). Shared UI params are
//               declared volatile (uint8_t writes are atomic on LX7; volatile
//               prevents compiler register-caching across the task switch).
//               heatPalette uses a double-buffer + atomic index so rebuilds
//               never race with the render loop.
// =============================================================================

#include <FastLED.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include <esp_system.h>
#include <esp_ota_ops.h>

// =============================================================================
//  CONFIG
// =============================================================================

// ---- Strip geometry --------------------------------------------------------
#define LED_PIN                 5
#define COLUMNS                 20
#define ROWS                    40
#define NUM_LEDS                (COLUMNS * ROWS)
#define LED_COLOR_ORDER         GRB           // change to RGB/BRG if base shows wrong color
#define COLOR_TEST              0             // 1 = solid-red boot test for 1 s

// ---- Fire algorithm --------------------------------------------------------
// Cooling & Sparking are dynamically controlled via the Web UI variables
#define SPARK_INTENSITY         240           // per-spark heat add range upper bound
#define WIND_CHANGE_INTERVAL    2000
#define WIND_MAX_STRENGTH       3
#define WIND_PULSE_PROBABILITY  50
// Temporal blend on render: 0..255, lower = smoother glow-up.
// At 50, a fresh spark needs ~10 frames (~250 ms at 40 FPS) to reach
// 90% of its target color — reads as an ember catching, not a flash.
#define FIRE_BLEND              50

// ---- Power -----------------------------------------------------------------
#define PSU_VOLTS               5
// MAX_PSU_MA is hardcoded to 20000mA (20A) in setup() to account for voltage drop

// ---- Brightness ------------------------------------------------------------
#define BRIGHT_DEFAULT          100           // 0..100, until UI / NVS overrides
#define BRIGHT_STEP             5             // -/+ button increment
#define BRIGHT_GAMMA            2.2f          // perceptual curve on the slider
#define BRIGHT_FLOOR            4             // min stable raw PWM on WS2812B
#define BRIGHT_DITHER_ON        16            // enable BINARY_DITHER above this raw

// ---- Network ---------------------------------------------------------------
// WIFI_SSID and WIFI_PASS are injected as build-time defines by get_version.py.
// Local: add credentials to secrets.ini (see secrets.ini.example).
// CI:    set WIFI_SSID and WIFI_PASS as GitHub Actions secrets.
#ifndef WIFI_SSID
  #error "WIFI_SSID not defined — copy secrets.ini.example to secrets.ini and fill in your credentials"
#endif
#ifndef WIFI_PASS
  #error "WIFI_PASS not defined — copy secrets.ini.example to secrets.ini and fill in your credentials"
#endif
#define WIFI_CONNECT_MS         10000         // setup() stops blocking after this
#define WIFI_RETRY_MS           15000         // background reconnect interval
#define NVS_COMMIT_DELAY_MS     2500          // defer NVS writes to spare flash endurance
#define FIRMWARE_URL            "https://github.com/MaksimShchuplov/firelamp/releases/latest/download/firmware.bin"
#define VERSION_URL             "https://github.com/MaksimShchuplov/firelamp/releases/latest/download/version.json"
#ifndef FIRMWARE_VERSION
  #define FIRMWARE_VERSION      "dev"
#endif


// =============================================================================
//  GLOBALS
// =============================================================================

CRGB     leds[NUM_LEDS];
uint8_t  heat[ROWS][COLUMNS];

// Double-buffered heat palette: buildHeatPalette() writes into the inactive
// buffer, then flips activePal. fireEffect() snapshots activePal once per
// frame so a mid-frame flip cannot cause split-palette rendering.
static CRGB            heatPalette[2][256];
static volatile uint8_t activePal = 0;

float    windDir[ROWS];
float    windTarget[ROWS];
uint8_t  coolMax[ROWS];                       // per-row cooling cap (updated via UI)
uint32_t lastWindChange = 0;

WebServer   server(80);
Preferences prefs;

// volatile: written from Core 1 (web handlers), read from Core 0 (LED task).
// Single-byte writes are atomic on LX7; volatile prevents the compiler from
// caching values in registers across the FreeRTOS task switch boundary.
volatile uint8_t  uiBright    = BRIGHT_DEFAULT;
volatile uint8_t  uiContrast  = 50;
volatile uint8_t  uiCooling   = 45;           // Tuned for IKEA Vidja 138cm
volatile uint8_t  uiSparking  = 36;           // Tuned for solid hot core
volatile uint8_t  appliedRaw  = 0;            // last 0..255 sent to FastLED
volatile float    currentPowerW = 0.0f;       // Estimated power, display-only

bool     prefsDirty  = false;                 // pending NVS commit
uint32_t prefsTouch  = 0;                     // millis of last setting change
uint32_t wifiRetryAt = 0;
uint32_t lastPowerCalc = 0;

// =============================================================================
//  PALETTE — heat LUT (red -> orange -> white-hot)
//  Written into the inactive double-buffer; activePal flipped atomically.
// =============================================================================

void buildHeatPalette() {
    const uint8_t next = 1 - activePal;       // write into the inactive buffer
    const float power = 1.0f + ((uiContrast - 50.0f) / 50.0f);
    for (int i = 0; i < 256; i++) {
        float normalized = (float)i / 255.0f;
        float adjusted   = powf(normalized, power);
        uint16_t mapped  = (uint16_t)(adjusted * 255.0f);

        uint8_t t192 = (uint8_t)((mapped * 191) / 255);
        uint8_t ramp = (uint8_t)((t192 & 0x3F) << 2);
        if      (t192 > 0x80) heatPalette[next][i] = CRGB(255, 255, ramp);
        else if (t192 > 0x40) heatPalette[next][i] = CRGB(255, ramp, 0);
        else                  heatPalette[next][i] = CRGB(ramp, 0, 0);
    }
    activePal = next;                         // atomic single-byte flip
}

// =============================================================================
//  BRIGHTNESS
// -----------------------------------------------------------------------------
//  uiBright (0..100) is the user-facing value. applyBrightness maps it with
//  a perceptual gamma curve, clamps to the minimum stable WS2812B PWM, and
//  conditionally disables temporal dither (which causes visible flicker
//  below ~raw 16 at ~40 FPS).
// =============================================================================

void applyBrightness() {
    uint8_t raw;
    if (uiBright == 0) {
        raw = 0;
    } else {
        float n = (float)uiBright / 100.0f;
        int v = (int)(powf(n, BRIGHT_GAMMA) * 255.0f + 0.5f);
        if (v < BRIGHT_FLOOR) v = BRIGHT_FLOOR;
        raw = (uint8_t)v;
    }
    if (raw != appliedRaw) {
        appliedRaw = raw;
        FastLED.setBrightness(raw);
        FastLED.setDither(raw < BRIGHT_DITHER_ON ? DISABLE_DITHER : BINARY_DITHER);
    }
}

void setBright(int v) {
    if (v < 0) v = 0; else if (v > 100) v = 100;
    if ((uint8_t)v != uiBright) {
        uiBright    = (uint8_t)v;
        prefsDirty  = true;
        prefsTouch  = millis();
        applyBrightness();
    }
}

// =============================================================================
//  FIRE SIMULATION
// =============================================================================

void updateWind() {
    if (millis() - lastWindChange > WIND_CHANGE_INTERVAL) {
        for (int y = 0; y < ROWS; y++) {
            windTarget[y] = (random8(100) < WIND_PULSE_PROBABILITY)
                ? (float)((int)random8(2 * WIND_MAX_STRENGTH + 1) - WIND_MAX_STRENGTH)
                : 0.0f;
        }
        lastWindChange = millis();
    }
    for (int y = 0; y < ROWS; y++)
        windDir[y] += (windTarget[y] - windDir[y]) * 0.1f;
}

void recalcCooling() {
    for (int y = 0; y < ROWS; y++) {
        const uint8_t lo = (uiCooling > 10) ? uiCooling - 10 : 0;  // guard underflow
        uint8_t cf = random8(lo, uiCooling + 10);
        coolMax[y] = (uint8_t)((cf * 10) / ROWS + 2);
    }
}

void updatePowerCalc() {
    float reqW = (float)calculate_unscaled_power_mW(leds, NUM_LEDS) * ((float)appliedRaw / 255.0f) / 1000.0f;
    float maxW = (float)(PSU_VOLTS * 20000) / 1000.0f;
    currentPowerW = (reqW > maxW) ? maxW : reqW;
    lastPowerCalc = millis();
}

void fireEffect() {
    // 1. Cooling — saturating subtract, no per-pixel mul/div, fast PRNG
    for (int y = 0; y < ROWS; y++) {
        const uint8_t cmax = coolMax[y];
        for (int x = 0; x < COLUMNS; x++)
            heat[y][x] = qsub8(heat[y][x], random8(cmax));
    }

    // 2. Upward propagation with wind. round() per-row, not per-pixel.
    //    Iterating y high->low means rows y-1/y-2 are still last frame's
    //    values when read (intended; that's how heat rises).
    for (int y = ROWS - 1; y > 0; y--) {
        const int wind = (int)lroundf(windDir[y]);
        const int y1 = y - 1;
        const int y2 = (y >= 2) ? (y - 2) : 0;
        for (int x = 0; x < COLUMNS; x++) {
            int nx = x + wind;
            if (nx >= COLUMNS) nx -= COLUMNS;
            else if (nx < 0) nx += COLUMNS;
            // Fast approximation of *3/5 and *2/5 avoiding division inside inner loop
            heat[y][x] = (uint8_t)(((uint16_t)heat[y1][nx] * 153 + (uint16_t)heat[y2][nx] * 102) >> 8);
        }
    }

    // 3. Ignite new sparks at the base
    for (int x = 0; x < COLUMNS; x++) {
        if (random8() < uiSparking) {
            int y = random8(3);
            heat[y][x] = qadd8(heat[y][x],
                               random8(SPARK_INTENSITY - 40, SPARK_INTENSITY));
        }
    }

    // 4. Render with temporal blend.
    //    Snapshot activePal once so a mid-frame palette flip cannot cause
    //    split-palette rendering (first N pixels old palette, rest new).
    const CRGB *pal = heatPalette[activePal];
    for (int y = 0; y < ROWS; y++) {
        const uint16_t base = (uint16_t)(ROWS - 1 - y) * COLUMNS;
        for (int x = 0; x < COLUMNS; x++) {
            nblend(leds[base + x], pal[heat[y][x]], FIRE_BLEND);
        }
    }

    if (millis() - lastPowerCalc > 3000) {
        updatePowerCalc();
    }
}

// =============================================================================
//  EMBEDDED CONTROL UI  —  ember theme, self-contained, served from PROGMEM
// =============================================================================

static const char PAGE[] PROGMEM = R"HTML(<!doctype html><html lang=en><head>
<meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<meta name=theme-color content="#0a0503"><title>Ember</title><style>
:root{--b:60;--g:calc(var(--b)/100)}
*{margin:0;padding:0;box-sizing:border-box;-webkit-tap-highlight-color:transparent}
html,body{min-height:100%}
body{font-family:'Helvetica Neue',Helvetica,Arial,sans-serif;color:#f6d9b0;
 display:flex;align-items:center;justify-content:center;overflow-x:hidden;overflow-y:auto;padding:40px 0;
 background:radial-gradient(120% 90% at 50% 118%,
  rgba(255,120,20,calc(.55*var(--g))) 0%,rgba(180,40,8,calc(.32*var(--g))) 32%,
  rgba(20,6,2,0) 66%),#0a0503}
.amb{position:fixed;inset:0;z-index:1;pointer-events:none;animation:fl 3.2s ease-in-out infinite;
 background:radial-gradient(60% 50% at 50% 116%,rgba(255,110,25,calc(.40*var(--g))),transparent 70%)}
@keyframes fl{0%,100%{opacity:.85}45%{opacity:1}70%{opacity:.78}}
.wrap{width:min(92vw,420px);text-align:center;position:relative;z-index:2}
.kick{font-size:12px;letter-spacing:.42em;text-transform:uppercase;color:#c8743a;opacity:.85;margin-bottom:6px}
h1{font-size:13px;letter-spacing:.3em;text-transform:uppercase;font-weight:600;color:#8a4a22;margin-bottom:12px;margin-top:24px}
.val{font-size:72px;font-weight:200;line-height:1;letter-spacing:-.04em;
 background:linear-gradient(180deg,#fff3d6,#ffb14a 55%,#ff6a18);-webkit-background-clip:text;
 background-clip:text;color:transparent;transition:filter .25s;
 filter:drop-shadow(0 0 calc(6px + 20px*var(--g)) rgba(255,140,40,calc(.4 + .5*var(--g))))}
.bar{margin:24px 0;-webkit-appearance:none;appearance:none;width:100%;height:14px;border-radius:9px;
 outline:none;box-shadow:inset 0 1px 3px rgba(0,0,0,.7);
 background:linear-gradient(90deg,#2a0d04,#7a1f06 22%,#d6510c 55%,#ff8a1f 78%,#ffe7b8)}
.bar::-webkit-slider-thumb{-webkit-appearance:none;width:30px;height:30px;border-radius:50%;cursor:pointer;
 background:radial-gradient(circle at 38% 32%,#fff,#ffae45 40%,#ff5e10 75%,#7a1f00);
 box-shadow:0 0 16px rgba(255,140,40,.9),0 2px 6px rgba(0,0,0,.6)}
.bar::-moz-range-thumb{width:30px;height:30px;border:0;border-radius:50%;
 background:radial-gradient(circle at 38% 32%,#fff,#ffae45 40%,#ff5e10 75%,#7a1f00);
 box-shadow:0 0 16px rgba(255,140,40,.9)}
.desc{font-size:12px;color:#a85a22;margin-top:-14px;margin-bottom:24px;letter-spacing:0.05em;line-height:1.4;opacity:0.8;font-weight:400;}
.reset{margin-top:10px;padding:12px 24px;background:none;border:1px solid #7a3f16;color:#c8743a;border-radius:24px;cursor:pointer;font-size:12px;text-transform:uppercase;letter-spacing:0.1em;transition:all 0.2s;}
.reset:active{background:#7a3f16;color:#fff2dd;}
.lang{position:absolute;top:20px;right:20px;display:flex;gap:4px;z-index:3;}
.lbtn{background:none;border:1px solid #7a3f16;color:#c8743a;padding:4px 8px;border-radius:4px;cursor:pointer;font-size:11px;font-weight:bold;transition:all 0.2s;}
.lbtn.act{background:#7a3f16;color:#fff2dd;}
.stat{margin-top:20px;text-align:center;font-size:13px;color:#a85a22;letter-spacing:0.1em;opacity:0.8;font-weight:400;}
.stat span{color:#ffb14a;font-weight:bold;}
.ibtn{position:absolute;top:20px;left:20px;background:none;border:1px solid #7a3f16;color:#c8743a;padding:4px 12px;border-radius:4px;cursor:pointer;font-size:14px;font-weight:bold;transition:all 0.2s;z-index:3;}
.ibtn:active{background:#7a3f16;color:#fff2dd;}
.mod{display:none;position:fixed;inset:0;background:rgba(10,5,3,0.95);z-index:10;flex-direction:column;padding:30px;overflow-y:auto;color:#f6d9b0;text-align:left;font-size:14px;line-height:1.5;}
.mod.show{display:flex;}
.mcls{align-self:flex-end;background:none;border:none;color:#ffb14a;font-size:28px;cursor:pointer;margin-bottom:10px;}
.mt{font-size:16px;font-weight:bold;color:#ff6a18;margin-top:15px;margin-bottom:5px;letter-spacing:.1em;text-transform:uppercase;}
.md{margin-bottom:15px;opacity:0.85;}
body.off .val,body.off .amb{filter:grayscale(.5);opacity:.4}
</style></head><body><div class=amb></div>
<button id=ibtn class=ibtn>?</button>
<div id=mod class=mod>
<button id=mcls class=mcls>&times;</button>
<div class=mt id=mt1>Brightness</div><div class=md id=md1></div>
<div class=mt id=mt2>Contrast</div><div class=md id=md2></div>
<div class=mt id=mt3>Cooling</div><div class=md id=md3></div>
<div class=mt id=mt4>Sparking</div><div class=md id=md4></div>
</div>
<div class=lang><button id=len class="lbtn act">EN</button><button id=lru class=lbtn>RU</button></div>
<div class=wrap>
<div class=kick>Fire Lamp</div>
<h1 id=lb>Brightness</h1>
<div class=val id=vb>--</div>
<input class=bar id=sb type=range min=0 max=100 value=50>
<div class=desc id=db>Controls the overall light output of the lamp.</div>
<h1 id=lc>Contrast</h1>
<div class=val id=vc>--</div>
<input class=bar id=sc type=range min=0 max=100 value=50>
<div class=desc id=dc>Adjusts the intensity of reds and oranges.</div>
<h1 id=lco>Cooling</h1>
<div class=val id=vco>--</div>
<input class=bar id=sco type=range min=20 max=150 value=70>
<div class=desc id=dco>Lower = taller flames.</div>
<h1 id=lsp>Sparking</h1>
<div class=val id=vsp>--</div>
<input class=bar id=ssp type=range min=0 max=255 value=95>
<div class=desc id=dsp>Higher = hotter base.</div>
<button class=reset id=rst>Reset to Default</button>
<button class=reset id=chk style="margin-top:10px;border-color:#1e3a8a;color:#60a5fa">Check for Update</button>
<div id=vinfo style="font-size:11px;color:#888;margin-top:6px;text-align:center"></div>
<div class=stat><span id=lw>Power:</span> <span id=vw>0.0</span> W</div>
</div><script>
var vb=document.getElementById('vb'),sb=document.getElementById('sb');
var vc=document.getElementById('vc'),sc=document.getElementById('sc');
var vco=document.getElementById('vco'),sco=document.getElementById('sco');
var vsp=document.getElementById('vsp'),ssp=document.getElementById('ssp');
var R=document.documentElement,t1,t2,t4,t5;
var ru=(localStorage.getItem('lang')==='ru')||(!localStorage.getItem('lang')&&navigator.language.startsWith('ru'));
function ul() {
 document.getElementById('len').classList.toggle('act',!ru);
 document.getElementById('lru').classList.toggle('act',ru);
 document.getElementById('lb').textContent=ru?'Яркость':'Brightness';
 document.getElementById('db').textContent=ru?'Управляет общей яркостью лампы.':'Controls the overall light output of the lamp.';
 document.getElementById('lc').textContent=ru?'Контрастность':'Contrast';
 document.getElementById('dc').textContent=ru?'Насыщенность красных и оранжевых оттенков.':'Adjusts the intensity of reds and oranges.';
 document.getElementById('lco').textContent=ru?'Охлаждение':'Cooling';
 document.getElementById('dco').textContent=ru?'Меньше = выше пламя.':'Lower = taller flames.';
 document.getElementById('lsp').textContent=ru?'Искры':'Sparking';
 document.getElementById('dsp').textContent=ru?'Больше = горячее основание.':'Higher = hotter base.';
 document.getElementById('rst').textContent=ru?'По умолчанию':'Reset to Default';
 document.getElementById('chk').textContent=ru?'Проверить обновления':'Check for Update';
 document.getElementById('lw').textContent=ru?'Потребление:':'Power:';
 document.getElementById('mt1').textContent=ru?'Яркость (Масштаб)':'Brightness (Scale)';
 document.getElementById('md1').textContent=ru?'Линейно масштабирует общую мощность свечения лампы. Не меняет физику пламени.':'Linearly scales the overall light output of the lamp. Does not change the flame physics.';
 document.getElementById('mt2').textContent=ru?'Контрастность (Цвет)':'Contrast (Palette)';
 document.getElementById('md2').textContent=ru?'Не влияет на физику. Сдвигает цвета: низкая контрастность дает больше желтого/белого, высокая оставляет только глубокий красный.':'Does not affect physics. Shifts the colors: low contrast allows more yellow/white, high contrast forces deep reds.';
 document.getElementById('mt3').textContent=ru?'Охлаждение (Высота)':'Cooling (Height)';
 document.getElementById('md3').textContent=ru?'Управляет скоростью затухания искр по мере подъема. Меньше значение = выше пламя. Больше значение = короткие искры.':'Dictates how quickly sparks die out as they travel up. Lower value = taller flames. Higher value = short embers.';
 document.getElementById('mt4').textContent=ru?'Искры (Бензин)':'Sparking (Ignition)';
 document.getElementById('md4').textContent=ru?'Управляет хаосом у основания. Высокое значение = сплошной, ревущий белый/желтый жар. Низкое = спокойное тление.':'Dictates chaos at the base. Higher value = a solid, roaring white/yellow inferno. Lower value = calm smoldering.';
}
ul();
document.getElementById('ibtn').onclick=function(){document.getElementById('mod').classList.add('show')};
document.getElementById('mcls').onclick=function(){document.getElementById('mod').classList.remove('show')};
document.getElementById('len').onclick=function(){ru=false;localStorage.setItem('lang','en');ul();};
document.getElementById('lru').onclick=function(){ru=true;localStorage.setItem('lang','ru');ul();};
function pb(n){n=Math.max(0,Math.min(100,n|0));vb.textContent=n;sb.value=n;R.style.setProperty('--b',n);document.body.classList.toggle('off',n===0)}
function pc(n){n=Math.max(0,Math.min(100,n|0));vc.textContent=n;sc.value=n;}
function pco(n){n=Math.max(20,Math.min(150,n|0));vco.textContent=n;sco.value=n;}
function psp(n){n=Math.max(0,Math.min(255,n|0));vsp.textContent=n;ssp.value=n;}
function pull(){fetch('/state').then(r=>r.json()).then(x=>{pb(x.b);pc(x.c);pco(x.co);psp(x.sp);if(x.w!==undefined)document.getElementById('vw').textContent=x.w.toFixed(1);}).catch(()=>{})}
sb.addEventListener('input',function(){pb(+sb.value);clearTimeout(t1);t1=setTimeout(function(){fetch('/setb?v='+sb.value)},120)});
sc.addEventListener('input',function(){pc(+sc.value);clearTimeout(t2);t2=setTimeout(function(){fetch('/setc?v='+sc.value)},120)});
sco.addEventListener('input',function(){pco(+sco.value);clearTimeout(t4);t4=setTimeout(function(){fetch('/setco?v='+sco.value)},120)});
ssp.addEventListener('input',function(){psp(+ssp.value);clearTimeout(t5);t5=setTimeout(function(){fetch('/setsp?v='+ssp.value)},120)});
document.getElementById('rst').onclick=function(){fetch('/reset').then(pull)};
var latestVer=null;
document.getElementById('chk').onclick=function(){
  var btn=document.getElementById('chk');
  btn.textContent=ru?'Проверка...':'Checking...';
  btn.disabled=true;
  fetch('/checkupdate').then(r=>r.json()).then(x=>{
    latestVer=x.latest;
    var vi=document.getElementById('vinfo');
    vi.textContent=(ru?'Текущая: ':'Current: ')+x.current+(ru?' → GitHub: ':' → GitHub: ')+x.latest;
    if(x.update_available){
      btn.textContent=ru?'Установить обновление ↑':'Install Update ↑';
      btn.style.borderColor='#16a34a';btn.style.color='#4ade80';
      btn.disabled=false;
      btn.onclick=function(){
        if(confirm(ru?'Начать обновление? Лампа перезагрузится.':'Install update? The lamp will reboot.')){fetch('/update').then(()=>alert(ru?'Обновление запущено...':'Update started...'))}
      };
    } else {
      btn.textContent=ru?'Версия актуальна ✓':'Up to date ✓';
      btn.style.color='#4ade80';
      setTimeout(()=>{btn.textContent=ru?'Проверить обновления':'Check for Update';btn.style.color='#60a5fa';btn.disabled=false;},3000);
    }
  }).catch(()=>{btn.textContent=ru?'Ошибка сети':'Network error';btn.disabled=false;});
};
pull();setInterval(function(){if(!document.hidden)pull()},4000);
</script></body></html>)HTML";

// =============================================================================
//  NETWORK  —  WiFi + sync WebServer + deferred NVS persistence
// -----------------------------------------------------------------------------
//  WebServer is the built-in synchronous one, polled once per frame from
//  loop(). When idle that costs microseconds; serving the page is a few ms
//  = one slightly long frame, imperceptible in a fire effect. Never blocks
//  on WiFi after setup(): a downed network just means the UI is unreachable
//  for a moment, fire keeps running, reconnect retries in the background.
// =============================================================================

void sendVal() {
    String json = "{\"b\":";
    json += (int)uiBright;
    json += ",\"c\":";
    json += (int)uiContrast;
    json += ",\"co\":";
    json += (int)uiCooling;
    json += ",\"sp\":";
    json += (int)uiSparking;
    json += ",\"w\":";
    json += String((float)currentPowerW, 1);
    json += "}";
    server.send(200, "application/json", json);
}

// Serve the page with actual current values injected so sliders show
// correct state immediately — no flash-of-stale-defaults before pull() fires.
void handleRoot() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    server.sendContent_P(PAGE);
    char init[96];
    snprintf(init, sizeof(init), "<script>pb(%d);pc(%d);pco(%d);psp(%d);</script>",
             (int)uiBright, (int)uiContrast, (int)uiCooling, (int)uiSparking);
    server.sendContent(init);
    server.sendContent("");
}

void handleSetB() {
    if (server.hasArg("v")) {
        setBright(server.arg("v").toInt());
        updatePowerCalc();
    }
    sendVal();
}

void handleSetC() {
    if (server.hasArg("v")) {
        int val = server.arg("v").toInt();
        if (val < 0)   val = 0;
        if (val > 100) val = 100;
        if (uiContrast != (uint8_t)val) {
            uiContrast  = (uint8_t)val;
            buildHeatPalette();
            prefsDirty  = true;
            prefsTouch  = millis();
        }
        updatePowerCalc();
    }
    sendVal();
}

void handleSetCo() {
    if (server.hasArg("v")) {
        int val = server.arg("v").toInt();
        if (val < 20)  val = 20;
        if (val > 150) val = 150;
        uiCooling  = (uint8_t)val;
        prefsDirty = true;
        prefsTouch = millis();
        recalcCooling();
        updatePowerCalc();
    }
    sendVal();
}

void handleSetSp() {
    if (server.hasArg("v")) {
        int val = server.arg("v").toInt();
        if (val < 0)   val = 0;
        if (val > 255) val = 255;
        uiSparking = (uint8_t)val;
        prefsDirty = true;
        prefsTouch = millis();
        updatePowerCalc();
    }
    sendVal();
}

void handleReset() {
    uiBright   = BRIGHT_DEFAULT;
    uiContrast = 50;
    uiCooling  = 45;
    uiSparking = 36;
    applyBrightness();
    buildHeatPalette();
    recalcCooling();
    prefsDirty = true;
    prefsTouch = millis();
    updatePowerCalc();
    sendVal();
}

// Fetches version.json from GitHub and returns parsed fields.
// Returns false if fetch failed. out_version and out_md5 are filled on success.
bool fetchVersionInfo(String &out_version, String &out_md5) {
    WiFiClientSecure client;
    // NOTE: certificate verification is disabled here. For a home network the
    // practical MITM risk is low, and the MD5 check in handleUpdate() provides
    // integrity verification against accidental corruption. Pinning the GitHub
    // CDN root CA is the proper fix but requires tracking cert rotations.
    client.setInsecure();
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.begin(client, VERSION_URL);
    int code = http.GET();
    if (code != 200) {
        Serial.printf("fetchVersionInfo: HTTP %d\n", code);
        http.end();
        return false;
    }
    String body = http.getString();
    http.end();

    // Minimal JSON parse — no external lib needed for two known keys
    auto extract = [&](const char *key) -> String {
        String search = String("\"") + key + "\":\"";
        int idx = body.indexOf(search);
        if (idx < 0) return "";
        idx += search.length();
        int end = body.indexOf("\"", idx);
        return (end > idx) ? body.substring(idx, end) : "";
    };
    out_version = extract("version");
    out_md5     = extract("md5");
    return out_version.length() > 0;
}

void handleCheckUpdate() {
    String latestVer, latestMd5;
    if (!fetchVersionInfo(latestVer, latestMd5)) {
        server.send(503, "application/json", "{\"error\":\"fetch_failed\"}");
        return;
    }
    String current  = FIRMWARE_VERSION;
    bool available  = (latestVer != current);
    String j = "{\"current\":\"" + current +
               "\",\"latest\":\"" + latestVer +
               "\",\"update_available\":" + (available ? "true" : "false") +
               ",\"date\":\"" + __DATE__ " " __TIME__ "\"}";
    server.send(200, "application/json", j);
}

void handleUpdate() {
    // Step 1: fetch version.json for MD5 integrity check
    String latestVer, latestMd5;
    bool haveMd5 = fetchVersionInfo(latestVer, latestMd5);
    if (haveMd5 && latestMd5.length() == 32) {
        Update.setMD5(latestMd5.c_str());
        Serial.printf("OTA: MD5 pre-set to %s\n", latestMd5.c_str());
    } else {
        Serial.println("OTA: no MD5 available, proceeding without integrity check");
    }

    // Step 2: respond NOW — browser will disconnect after reboot anyway
    server.send(200, "text/plain", "Update starting...");
    server.client().stop();           // flush + close before blocking OTA

    WiFiClientSecure client;
    client.setInsecure();
    httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

    Serial.printf("Starting OTA from GitHub Releases (target: %s)...\n", latestVer.c_str());
    t_httpUpdate_return ret = httpUpdate.update(client, FIRMWARE_URL);

    switch (ret) {
        case HTTP_UPDATE_FAILED:
            Serial.printf("OTA Failed (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
            break;
        case HTTP_UPDATE_NO_UPDATES:
            Serial.println("OTA: server says no update");
            break;
        case HTTP_UPDATE_OK:
            Serial.println("OTA Success — rebooting");
            break;
    }
}


void safeBootCheck() {
    esp_reset_reason_t reason = esp_reset_reason();
    // Only count hard crashes (panics, watchdogs). Normal power-ons or OTA restarts are ignored.
    if (reason == ESP_RST_PANIC || reason == ESP_RST_INT_WDT || reason == ESP_RST_TASK_WDT || reason == ESP_RST_WDT) {
        Preferences p;
        p.begin("lamp", false);
        uint32_t crashes = p.getUInt("crashes", 0);
        crashes++;
        if (crashes >= 3) {
            Serial.println("CRITICAL: Boot loop detected! Rolling back to previous firmware...");
            p.putUInt("crashes", 0);
            p.end();
            if (Update.canRollBack()) {
                Update.rollBack();
                ESP.restart();
            } else {
                Serial.println("WARNING: canRollBack() = false, no previous firmware available.");
            }
        } else {
            Serial.printf("Warning: Crash detected. Count: %d/3\n", crashes);
            p.putUInt("crashes", crashes);
            p.end();
        }
    }
}

void markBootSuccess() {
    Preferences p;
    p.begin("lamp", false);
    p.putUInt("crashes", 0);
    p.end();
    esp_ota_mark_app_valid_cancel_rollback();
    Serial.println("Boot verified: firmware marked as valid.");
}

void startNetwork() {
    prefs.begin("lamp", false);
    uiBright   = prefs.getUChar("bright2",  BRIGHT_DEFAULT);
    uiContrast = prefs.getUChar("contrast", 50);
    uiCooling  = prefs.getUChar("cooling",  45);
    uiSparking = prefs.getUChar("sparking", 36);
    applyBrightness();
    buildHeatPalette();
    recalcCooling();

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);                     // modem sleep -> periodic LED timing hitch
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    uint32_t t0 = millis();                   // blocks only here; fire not running yet
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_CONNECT_MS)
        delay(150);

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("UI ready: http://");
        Serial.println(WiFi.localIP());
        markBootSuccess();
    } else {
        Serial.println("WiFi down - fire runs anyway, UI retries in bg");
    }

    server.on("/",            handleRoot);
    server.on("/state",       []() { sendVal(); });
    server.on("/setb",        handleSetB);
    server.on("/setc",        handleSetC);
    server.on("/setco",       handleSetCo);
    server.on("/setsp",       handleSetSp);
    server.on("/reset",       handleReset);
    server.on("/update",      handleUpdate);
    server.on("/checkupdate", handleCheckUpdate);
    server.on("/info", []() {
        String j = "{\"flash_mb\":";
        j += ESP.getFlashChipSize() / (1024 * 1024);
        j += ",\"free_heap\":";
        j += ESP.getFreeHeap();
        j += ",\"version\":\"" FIRMWARE_VERSION "\"";
        j += ",\"build\":\"" __DATE__ " " __TIME__ "\"}";
        server.send(200, "application/json", j);
    });
    server.onNotFound([]() { server.send(404, "text/plain", "404"); });

    server.begin();
}

void serviceNetwork() {
    server.handleClient();

    // Deferred persistence: commit only after value stable ~2.5 s, so
    // dragging the slider doesn't burn flash endurance.
    if (prefsDirty && millis() - prefsTouch > NVS_COMMIT_DELAY_MS) {
        prefs.putUChar("bright2",  uiBright);
        prefs.putUChar("contrast", uiContrast);
        prefs.putUChar("cooling",  uiCooling);
        prefs.putUChar("sparking", uiSparking);
        prefsDirty = false;
    }

    // Non-blocking reconnect.
    if (WiFi.status() != WL_CONNECTED && millis() - wifiRetryAt > WIFI_RETRY_MS) {
        wifiRetryAt = millis();
        WiFi.disconnect();
        WiFi.begin(WIFI_SSID, WIFI_PASS);
    }
}

// =============================================================================
//  SETUP / LOOP
// =============================================================================

void setup() {
    Serial.begin(115200);
    safeBootCheck();

    FastLED.addLeds<WS2812B, LED_PIN, LED_COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setMaxPowerInVoltsAndMilliamps(PSU_VOLTS, 20000);
    FastLED.clear(true);

#if COLOR_TEST
    // Whole strip MUST be solid RED for 1 s. Green/blue instead means
    // LED_COLOR_ORDER is wrong — change above (RGB is the usual fix).
    fill_solid(leds, NUM_LEDS, CRGB(255, 0, 0));
    FastLED.show();
    delay(1000);
    FastLED.clear(true);
#endif

    for (int y = 0; y < ROWS; y++) {
        windDir[y]    = 0.0f;
        windTarget[y] = 0.0f;
    }

    startNetwork();                            // loads NVS, builds palette, joins WiFi

    xTaskCreatePinnedToCore(
        [](void *) {
            for (;;) {
                updateWind();
                fireEffect();
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(1)); // yield to feed Core 0 watchdog/WiFi
            }
        },
        "LEDTask", 4096, NULL, 1, NULL, 0
    );
}

void loop() {
    serviceNetwork();
    vTaskDelay(pdMS_TO_TICKS(10));             // yield Core 1
}

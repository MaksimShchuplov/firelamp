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
//  Power:     800 LEDs full-white = ~48 A. Mean Well 60 W = 12 A. FastLED's
//             real-time current limiter (setMaxPowerInVoltsAndMilliamps) is
//             what keeps a spark burst from sagging or tripping the rail.
// =============================================================================

#include <FastLED.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>

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
#define COOLING                 70
#define SPARKING                95
#define SPARK_INTENSITY         240           // per-spark heat add range upper bound
                                              // (was 180; lower = smaller per-frame jumps)
#define WIND_CHANGE_INTERVAL    2000
#define WIND_MAX_STRENGTH       3
#define WIND_PULSE_PROBABILITY  50
// Temporal blend on render: 0..255, lower = smoother glow-up.
// At 50, a fresh spark needs ~10 frames (~250 ms at 40 FPS) to reach
// 90% of its target color — reads as an ember catching, not a flash.
#define FIRE_BLEND              50

// ---- Power -----------------------------------------------------------------
#define PSU_VOLTS               5
#define MAX_PSU_MA              12000         // Mean Well 60 W @ 5 V, full rating

// ---- Brightness ------------------------------------------------------------
#define BRIGHT_DEFAULT          100           // 0..100, until UI / NVS overrides
#define BRIGHT_STEP             5             // -/+ button increment
#define BRIGHT_GAMMA            2.2f          // perceptual curve on the slider
#define BRIGHT_FLOOR            4             // min stable raw PWM on WS2812B
#define BRIGHT_DITHER_ON        16            // enable BINARY_DITHER above this raw

// ---- Network ---------------------------------------------------------------
// NOTE: credentials are compiled into flash as clear text. Fine for a home
// lamp; don't treat the .bin as a secret.
#define WIFI_SSID               "WIFI_SSID_REMOVED"
#define WIFI_PASS               "WIFI_PASS_REMOVED"
#define WIFI_CONNECT_MS         10000         // setup() stops blocking after this
#define WIFI_RETRY_MS           15000         // background reconnect interval
#define NVS_COMMIT_DELAY_MS     2500          // defer brightness writes to spare flash

// =============================================================================
//  GLOBALS
// =============================================================================

CRGB     leds[NUM_LEDS];
uint8_t  heat[ROWS][COLUMNS];
CRGB     heatPalette[256];

float    windDir[ROWS];
float    windTarget[ROWS];
uint8_t  coolMax[ROWS];                       // per-row cooling cap (constant after init)
uint32_t lastWindChange = 0;

WebServer   server(80);
Preferences prefs;
uint8_t  uiBright    = BRIGHT_DEFAULT;        // 0..100, user-facing
uint8_t  appliedRaw  = 0;                     // last 0..255 sent to FastLED
bool     brightDirty = false;                 // pending NVS commit
uint32_t brightTouch = 0;                     // millis of last brightness change
uint32_t wifiRetryAt = 0;

// =============================================================================
//  PALETTE — heat LUT (red -> orange -> white-hot), built once at boot
// =============================================================================

void buildHeatPalette() {
    for (int i = 0; i < 256; i++) {
        uint8_t t192 = (uint8_t)(((uint16_t)i * 191) / 255);
        uint8_t ramp = (uint8_t)((t192 & 0x3F) << 2);
        if      (t192 > 0x80) heatPalette[i] = CRGB(255, 255, ramp);  // white-hot
        else if (t192 > 0x40) heatPalette[i] = CRGB(255, ramp, 0);    // orange
        else                  heatPalette[i] = CRGB(ramp, 0, 0);      // red
    }
}

// =============================================================================
//  BRIGHTNESS
// -----------------------------------------------------------------------------
//  uiBright (0..100) is the user-facing value. applyBrightness maps it with
//  a perceptual gamma curve, clamps to the minimum stable WS2812B PWM, and
//  conditionally disables temporal dither (which causes visible flicker
//  below ~raw 16 at ~40 FPS). The brightness-aware palette is also rebuilt.
//  The MAX_PSU_MA cap continues to govern real current draw on top of this.
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
        brightDirty = true;
        brightTouch = millis();
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
            // Fast approximation of * 3/5 and * 2/5 avoiding hardware division inside inner loop
            heat[y][x] = (uint8_t)(((uint16_t)heat[y1][nx] * 153 + (uint16_t)heat[y2][nx] * 102) >> 8);
        }
    }

    // 3. Base sparks (saturating add). The heat skyrockets in one frame —
    //    the temporal blend in step 4 is what turns that snap into a glow-up.
    for (int x = 0; x < COLUMNS; x++) {
        if (random8() < SPARKING) {
            int y = random8(3);
            heat[y][x] = qadd8(heat[y][x],
                               random8(SPARK_INTENSITY - 40, SPARK_INTENSITY));
        }
    }

    // 4. Render with temporal blend: leds[i] walks toward the target heat
    //    color at FIRE_BLEND/255 per frame instead of jumping. Without this
    //    the spark step shows up as a hard flash at the base on every
    //    rebirth (visible mostly below ~80% brightness, which is exactly
    //    what the user was reporting).
    for (int y = 0; y < ROWS; y++) {
        const uint16_t base = (uint16_t)(ROWS - 1 - y) * COLUMNS;
        for (int x = 0; x < COLUMNS; x++) {
            nblend(leds[base + x], heatPalette[heat[y][x]], FIRE_BLEND);
        }
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
html,body{height:100%}
body{font-family:'Helvetica Neue',Helvetica,Arial,sans-serif;color:#f6d9b0;
 display:flex;align-items:center;justify-content:center;overflow:hidden;
 background:radial-gradient(120% 90% at 50% 118%,
  rgba(255,120,20,calc(.55*var(--g))) 0%,rgba(180,40,8,calc(.32*var(--g))) 32%,
  rgba(20,6,2,0) 66%),#0a0503}
.amb{position:fixed;inset:0;z-index:1;pointer-events:none;animation:fl 3.2s ease-in-out infinite;
 background:radial-gradient(60% 50% at 50% 116%,rgba(255,110,25,calc(.40*var(--g))),transparent 70%)}
@keyframes fl{0%,100%{opacity:.85}45%{opacity:1}70%{opacity:.78}}
.wrap{width:min(92vw,420px);text-align:center;position:relative;z-index:2}
.kick{font-size:12px;letter-spacing:.42em;text-transform:uppercase;color:#c8743a;opacity:.85;margin-bottom:6px}
h1{font-size:13px;letter-spacing:.3em;text-transform:uppercase;font-weight:600;color:#8a4a22;margin-bottom:44px}
.val{font-size:108px;font-weight:200;line-height:1;letter-spacing:-.04em;
 background:linear-gradient(180deg,#fff3d6,#ffb14a 55%,#ff6a18);-webkit-background-clip:text;
 background-clip:text;color:transparent;transition:filter .25s;
 filter:drop-shadow(0 0 calc(6px + 34px*var(--g)) rgba(255,140,40,calc(.4 + .5*var(--g))))}
.unit{font-size:13px;letter-spacing:.35em;color:#9c5a2c;margin-top:8px;text-transform:uppercase}
.bar{margin:42px 0 38px;-webkit-appearance:none;appearance:none;width:100%;height:14px;border-radius:9px;
 outline:none;box-shadow:inset 0 1px 3px rgba(0,0,0,.7);
 background:linear-gradient(90deg,#2a0d04,#7a1f06 22%,#d6510c 55%,#ff8a1f 78%,#ffe7b8)}
.bar::-webkit-slider-thumb{-webkit-appearance:none;width:30px;height:30px;border-radius:50%;cursor:pointer;
 background:radial-gradient(circle at 38% 32%,#fff,#ffae45 40%,#ff5e10 75%,#7a1f00);
 box-shadow:0 0 16px rgba(255,140,40,.9),0 2px 6px rgba(0,0,0,.6)}
.bar::-moz-range-thumb{width:30px;height:30px;border:0;border-radius:50%;
 background:radial-gradient(circle at 38% 32%,#fff,#ffae45 40%,#ff5e10 75%,#7a1f00);
 box-shadow:0 0 16px rgba(255,140,40,.9)}
.row{display:flex;gap:26px;justify-content:center}
.btn{width:96px;height:96px;border:0;border-radius:50%;cursor:pointer;font-size:38px;font-weight:300;
 color:#fff2dd;background:radial-gradient(circle at 40% 34%,#5a2208,transparent 70%),#1b0a03;
 transition:transform .08s,box-shadow .2s;
 box-shadow:0 10px 24px rgba(0,0,0,.6),inset 0 0 22px rgba(255,110,30,.28),
  inset 0 1px 1px rgba(255,180,90,.25)}
.btn:active{transform:scale(.93);box-shadow:0 0 0 1px #000,inset 0 0 30px rgba(255,120,30,.5)}
body.off .val,body.off .amb{filter:grayscale(.5);opacity:.4}
</style></head><body><div class=amb></div><div class=wrap>
<div class=kick>Fire Lamp</div><h1>Brightness</h1>
<div class=val id=v>60</div><div class=unit>percent</div>
<input class=bar id=s type=range min=0 max=100 value=60>
<div class=row><button class=btn id=dn>&minus;</button><button class=btn id=up>&plus;</button></div>
</div><script>
var v=document.getElementById('v'),s=document.getElementById('s'),R=document.documentElement,t;
function paint(n){n=Math.max(0,Math.min(100,n|0));v.textContent=n;s.value=n;
 R.style.setProperty('--b',n);document.body.classList.toggle('off',n===0)}
function pull(u){fetch(u).then(r=>r.text()).then(x=>paint(parseInt(x,10))).catch(()=>{})}
document.getElementById('up').onclick=function(){pull('/up')};
document.getElementById('dn').onclick=function(){pull('/down')};
s.addEventListener('input',function(){paint(+s.value);
 clearTimeout(t);t=setTimeout(function(){pull('/set?v='+s.value)},120)});
pull('/state');setInterval(function(){if(!document.hidden)pull('/state')},4000);
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

void sendVal()      { server.send(200, "text/plain", String((int)uiBright)); }
void handleRoot()   { server.send_P(200, "text/html", PAGE); }
void handleUp()     { setBright(uiBright + BRIGHT_STEP); sendVal(); }
void handleDown()   { setBright(uiBright - BRIGHT_STEP); sendVal(); }
void handleSet()    { if (server.hasArg("v")) setBright(server.arg("v").toInt());
                      sendVal(); }

void startNetwork() {
    prefs.begin("lamp", false);
    uiBright = prefs.getUChar("bright2", BRIGHT_DEFAULT);
    applyBrightness();

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);                     // modem sleep -> periodic LED timing hitch
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    uint32_t t0 = millis();                   // blocks only here; fire not running yet
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < WIFI_CONNECT_MS)
        delay(150);

    if (WiFi.status() == WL_CONNECTED) {
        Serial.print("UI ready: http://");
        Serial.println(WiFi.localIP());
    } else {
        Serial.println("WiFi down - fire runs anyway, UI retries in bg");
    }

    server.on("/",      handleRoot);
    server.on("/state", []() { sendVal(); });
    server.on("/set",   handleSet);
    server.on("/up",    handleUp);
    server.on("/down",  handleDown);
    server.onNotFound([]() { server.send(404, "text/plain", "404"); });
    server.begin();
}

void serviceNetwork() {
    server.handleClient();

    // Deferred persistence: commit only after value stable ~2.5 s, so
    // holding -/+ doesn't burn flash endurance.
    if (brightDirty && millis() - brightTouch > NVS_COMMIT_DELAY_MS) {
        prefs.putUChar("bright2", uiBright);
        brightDirty = false;
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

    FastLED.addLeds<WS2812B, LED_PIN, LED_COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setMaxPowerInVoltsAndMilliamps(PSU_VOLTS, MAX_PSU_MA);
    FastLED.clear(true);

#if COLOR_TEST
    // Whole strip MUST be solid RED for 1 s. Green/blue instead means
    // LED_COLOR_ORDER is wrong — change above (RGB is the usual fix).
    fill_solid(leds, NUM_LEDS, CRGB(255, 0, 0));
    FastLED.show();
    delay(1000);
    FastLED.clear(true);
#endif

    buildHeatPalette();

    for (int y = 0; y < ROWS; y++) {
        windDir[y]    = 0.0f;
        windTarget[y] = 0.0f;
        uint8_t cf    = random8(COOLING - 10, COOLING + 10);
        coolMax[y]    = (uint8_t)((cf * 10) / ROWS + 2);
    }

    startNetwork();                            // loads brightness, joins WiFi, starts UI
}

void loop() {
    serviceNetwork();
    updateWind();
    fireEffect();
    FastLED.show();
    // No delay(): single-line WS2812B at 800 LEDs self-paces at ~40 FPS
    // (24 ms of protocol time per frame). RMT5 async overlaps the next
    // frame's compute with the current frame's transmission for free.
}

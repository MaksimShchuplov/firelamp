#include <Arduino.h>
#include <WiFi.h>
#include <Update.h>
#include "globals.h"
#include "net_helpers.h"
#include "page.h"

// =============================================================================
//  SHARED UTILITIES
// =============================================================================

String jsonEscape(const String &s) {
    String r;
    r.reserve(s.length() + 8);
    for (unsigned i = 0; i < s.length(); i++) {
        char c = s[i];
        if      (c == '"')          r += "\\\"";
        else if (c == '\\')         r += "\\\\";
        else if (c == '\n')         r += "\\n";
        else if (c == '\r')         r += "\\r";
        else if (c == '\t')         r += "\\t";
        else if ((uint8_t)c < 0x20) { /* skip control characters */ }
        else                        r += c;
    }
    return r;
}

// Rejects requests without our custom header — blocks cross-origin CSRF attempts.
// Browser CORS pre-flight fails for cross-origin requests that set custom headers,
// so any request that reaches this check with the header set came from our own page.
bool isWebRequest() {
    if (server.header("X-Requested-With") != "firelamp") {
        server.send(403, "text/plain", "Forbidden");
        return false;
    }
    return true;
}

bool parseIntArg(const char *name, int lo, int hi, int &out) {
    if (!server.hasArg(name)) return false;
    const String &s = server.arg(name);
    if (s.isEmpty()) return false;
    // Reject obviously non-numeric strings before toInt() silently returns 0.
    bool hasDigit = false;
    for (unsigned i = (s[0] == '-') ? 1 : 0; i < s.length(); i++)
        if (isdigit(s[i])) { hasDigit = true; break; }
    if (!hasDigit) return false;
    long v = s.toInt();
    if (v < lo || v > hi) return false;
    out = (int)v;
    return true;
}

void formatState(char *buf, size_t len) {
    snprintf(buf, len,
             "{\"b\":%d,\"c\":%d,\"co\":%d,\"sp\":%d,\"w\":%.1f,\"bl\":%d,\"th\":%d,\"upd\":%d}",
             (int)uiBright, (int)uiContrast, (int)uiCooling, (int)uiSparking,
             (float)currentPowerMw / 1000.0f, (int)uiBlend, (int)uiTheme, updatePending ? 1 : 0);
}

void sendVal() {
    char j[96];
    formatState(j, sizeof(j));
    server.send(200, "application/json", j);
}

// Uses the always-open global prefs handle (opened in startNetwork).
bool flushPrefs() {
    bool ok = true;
    ok &= (prefs.putUChar("bright2",  uiBright)  != 0);
    ok &= (prefs.putUChar("contrast", uiContrast) != 0);
    ok &= (prefs.putUChar("cooling",  uiCooling)  != 0);
    ok &= (prefs.putUChar("sparking", uiSparking) != 0);
    ok &= (prefs.putUChar("blend",    uiBlend)    != 0);
    ok &= (prefs.putUChar("theme",    uiTheme)    != 0);
    return ok;
}

// =============================================================================
//  HTTP HANDLERS
// =============================================================================

static void sendInvalid() { server.send(400, "application/json", "{\"error\":\"invalid\"}"); }

static void handleRoot() {
    server.sendHeader("X-Content-Type-Options", "nosniff");
    server.sendHeader("X-Frame-Options", "SAMEORIGIN");
    server.sendHeader("Referrer-Policy", "no-referrer");
    server.sendHeader("Content-Security-Policy",
        "default-src 'self'; style-src 'self' 'unsafe-inline'; script-src 'self' 'unsafe-inline'; object-src 'none'; base-uri 'self'");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    server.sendContent_P(PAGE);
    char init[96];
    snprintf(init, sizeof(init), "<script>pb(%d);pc(%d);pco(%d);psp(%d);pbl(%d);pth(%d);</script>",
             (int)uiBright, (int)uiContrast, (int)uiCooling, (int)uiSparking,
             (int)uiBlend, (int)uiTheme);
    server.sendContent(init);
    server.sendContent("");
}

static void handleSetB() {
    if (!isWebRequest()) return;
    int v;
    if (!parseIntArg("v", 0, 100, v)) { sendInvalid(); return; }
    setBright(v); updatePowerCalc();
    sendVal();
}

static void handleSetC() {
    if (!isWebRequest()) return;
    int v;
    if (!parseIntArg("v", 0, 100, v)) { sendInvalid(); return; }
    if (uiContrast != (uint8_t)v) {
        uiContrast = (uint8_t)v;
        buildHeatPalette();
        markDirty();
    }
    updatePowerCalc();
    sendVal();
}

static void handleSetCo() {
    if (!isWebRequest()) return;
    int v;
    if (!parseIntArg("v", 20, 150, v)) { sendInvalid(); return; }
    if (uiCooling != (uint8_t)v) {
        uiCooling = (uint8_t)v;
        recalcCooling();
        markDirty();
    }
    updatePowerCalc();
    sendVal();
}

static void handleSetSp() {
    if (!isWebRequest()) return;
    int v;
    if (!parseIntArg("v", 0, 255, v)) { sendInvalid(); return; }
    if (uiSparking != (uint8_t)v) {
        uiSparking = (uint8_t)v;
        markDirty();
    }
    updatePowerCalc();
    sendVal();
}

static void handleSetBl() {
    if (!isWebRequest()) return;
    int v;
    if (!parseIntArg("v", 0, 255, v)) { sendInvalid(); return; }
    if (uiBlend != (uint8_t)v) {
        uiBlend = (uint8_t)v;
        markDirty();
    }
    updatePowerCalc();
    sendVal();
}

static void handleSetTheme() {
    if (!isWebRequest()) return;
    int v;
    if (!parseIntArg("v", 0, THEME_COUNT - 1, v)) { sendInvalid(); return; }
    if (uiTheme != (uint8_t)v) {
        uiTheme = (uint8_t)v;
        buildHeatPalette();
        markDirty();
    }
    updatePowerCalc();
    sendVal();
}

static void handleReset() {
    if (!isWebRequest()) return;
    uiBright = BRIGHT_DEFAULT; uiContrast = CONTRAST_DEFAULT;
    uiCooling = COOLING_DEFAULT; uiSparking = SPARKING_DEFAULT;
    uiBlend = BLEND_DEFAULT; uiTheme = THEME_DEFAULT;
    buildHeatPalette(); recalcCooling();
    markDirty();
    updatePowerCalc(); sendVal();
}

static void handleInfo() {
    char j[768];  // 512 is tight for 32-char escaped SSID + all fields; 768 gives safe headroom
    String ip   = WiFi.localIP().toString();
    String ssid = jsonEscape(WiFi.SSID());
    uint32_t watermark = ledTaskHandle ? uxTaskGetStackHighWaterMark(ledTaskHandle) : 0;
    snprintf(j, sizeof(j),
             "{\"flash_mb\":%u,\"free_heap\":%u,\"min_heap\":%u,"
             "\"rssi_dbm\":%d,\"uptime_s\":%lu,\"ip\":\"%s\",\"ssid\":\"%s\","
             "\"led_stack_free\":%lu,"
             "\"version\":\"" FIRMWARE_VERSION "\",\"build\":\"" __DATE__ " " __TIME__ "\"}",
             (unsigned)(ESP.getFlashChipSize() / (1024 * 1024)),
             (unsigned)ESP.getFreeHeap(),
             (unsigned)ESP.getMinFreeHeap(),
             (int)WiFi.RSSI(),
             (unsigned long)(millis() / 1000),
             ip.c_str(), ssid.c_str(),
             (unsigned long)watermark);
    server.send(200, "application/json", j);
}

static void handleDebug() {
    if (!isWebRequest()) return;
    char j[768];
    uint32_t watermark = ledTaskHandle ? uxTaskGetStackHighWaterMark(ledTaskHandle) : 0;
    String   ssid      = jsonEscape(WiFi.SSID());
    String   ip        = WiFi.localIP().toString();
    snprintf(j, sizeof(j),
             "{\"uptime_s\":%lu,\"free_heap\":%u,\"min_heap\":%u,"
             "\"led_stack_free\":%lu,\"task_count\":%u,"
             "\"rssi_dbm\":%d,\"ssid\":\"%s\",\"ip\":\"%s\","
             "\"b\":%d,\"c\":%d,\"co\":%d,\"sp\":%d,\"bl\":%d,\"th\":%d,"
             "\"bright_raw\":%d,\"power_mw\":%lu,\"upd\":%d,"
             "\"version\":\"" FIRMWARE_VERSION "\"}",
             (unsigned long)(millis() / 1000),
             (unsigned)ESP.getFreeHeap(),
             (unsigned)ESP.getMinFreeHeap(),
             (unsigned long)watermark,
             (unsigned)uxTaskGetNumberOfTasks(),
             (int)WiFi.RSSI(), ssid.c_str(), ip.c_str(),
             (int)uiBright, (int)uiContrast, (int)uiCooling,
             (int)uiSparking, (int)uiBlend, (int)uiTheme,
             (int)appliedRaw, (unsigned long)currentPowerMw,
             updatePending ? 1 : 0);
    server.send(200, "application/json", j);
}

static String htmlEscape(const char *s) {
    String r;
    for (; *s; s++) {
        if      (*s == '&')  r += F("&amp;");
        else if (*s == '<')  r += F("&lt;");
        else if (*s == '>')  r += F("&gt;");
        else if (*s == '"')  r += F("&quot;");
        else                 r += *s;
    }
    return r;
}

static void handleLog() {
    static char snap[LOG_BUF_LINES][LOG_BUF_WIDTH];
    int  snapHead;
    portENTER_CRITICAL(&logMux);
    memcpy(snap, logBuf, sizeof(snap));
    snapHead = logHead;
    portEXIT_CRITICAL(&logMux);

    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    server.sendContent(F("<!doctype html><meta name='viewport' content='width=device-width'>"
                   "<style>body{background:#111;color:#ccc;font:13px monospace;padding:8px}"
                   "pre{white-space:pre-wrap;word-break:break-all}</style><pre>"));
                   
    for (int i = 0; i < LOG_BUF_LINES; i++) {
        int idx = (snapHead + i) % LOG_BUF_LINES;
        if (snap[idx][0]) {
            server.sendContent(htmlEscape(snap[idx]));
            server.sendContent("\n");
        }
    }
    server.sendContent(F("</pre><script>setTimeout(()=>location.reload(),5000)</script>"));
    server.sendContent("");
}

static void handleResetWifi() {
    if (!isWebRequest()) return;
    server.send(200, "text/plain", "WiFi cleared — rebooting into setup mode...");
    server.client().stop();
    delay(200);
    wm.resetSettings();
    ESP.restart();
}

static void handleState() {
    char j[96];
    formatState(j, sizeof(j));
    server.send(200, "application/json", j);
}

// =============================================================================
//  MANUAL FIRMWARE FLASH  (/flash)
//  Recovery endpoint: user downloads firmware.bin from GitHub Releases in their
//  browser, then uploads it directly to the lamp over local HTTP — no TLS to
//  GitHub required.  Useful when the OTA-to-GitHub path is broken.
// =============================================================================

static bool s_flashAuth = false;

static void handleFlashPage() {
    server.send(200, "text/html", F(
        "<!doctype html><meta name='viewport' content='width=device-width'>"
        "<style>body{font:15px sans-serif;max-width:420px;margin:40px auto;"
        "padding:0 16px;background:#111;color:#eee}h2{color:#f80}a{color:#f80}"
        "button{padding:10px 20px;background:#c44;color:#fff;border:none;"
        "border-radius:4px;cursor:pointer;font-size:15px}button:disabled{opacity:.4}"
        "pre{background:#222;padding:10px;border-radius:4px;white-space:pre-wrap}"
        "input{display:block;margin:12px 0}</style>"
        "<h2>Manual Firmware Flash</h2>"
        "<p>1. Download <code>firmware.bin</code> from "
        "<a href='https://github.com/maksimshchuplov/firelamp/releases/latest'"
        " target='_blank'>GitHub Releases</a>.</p>"
        "<p>2. Select the file below and click Upload.</p>"
        "<input type='file' id='f' accept='.bin'>"
        "<button id='b' onclick='up()'>Upload</button>"
        "<pre id='s'></pre>"
        "<script>"
        "function up(){"
        "var f=document.getElementById('f').files[0];"
        "if(!f){alert('Select firmware.bin first');return;}"
        "if(f.size<50000){alert('File too small — wrong file?');return;}"
        "var s=document.getElementById('s');"
        "document.getElementById('b').disabled=true;"
        "s.textContent='Uploading '+Math.round(f.size/1024)+' KB…';"
        "var fd=new FormData();fd.append('firmware',f);"
        "fetch('/flash',{method:'POST',headers:{'X-Requested-With':'firelamp'},body:fd})"
        ".then(r=>r.text().then(t=>({ok:r.ok,t})))"
        ".then(({ok,t})=>{"
        "if(ok){s.textContent=t+'\\nReloading in 8 s…';"
        "setTimeout(()=>location.href='/',8000);}"
        "else{s.textContent='Error: '+t;"
        "document.getElementById('b').disabled=false;}})"
        ".catch(()=>{"
        "s.textContent='Upload sent — lamp rebooting.\\nReloading in 10 s…';"
        "setTimeout(()=>location.href='/',10000);});}"
        "</script>"
    ));
}

static void handleFlashUpload() {
    HTTPUpload &up = server.upload();
    if (up.status == UPLOAD_FILE_START) {
        s_flashAuth = (server.header("X-Requested-With") == "firelamp");
        if (s_flashAuth && !Update.begin(UPDATE_SIZE_UNKNOWN)) {
            LOG_ERROR("Flash: Update.begin: %s", Update.errorString());
            s_flashAuth = false;
        }
    } else if (up.status == UPLOAD_FILE_WRITE) {
        if (s_flashAuth) Update.write(up.buf, up.currentSize);
    } else if (up.status == UPLOAD_FILE_END) {
        if (s_flashAuth && !Update.end(true))
            LOG_ERROR("Flash: Update.end: %s", Update.errorString());
    } else if (up.status == UPLOAD_FILE_ABORTED) {
        if (s_flashAuth) Update.abort();
        s_flashAuth = false;
    }
}

static void handleFlashDone() {
    if (!s_flashAuth) {
        server.send(403, "text/plain", "Forbidden");
        return;
    }
    if (Update.hasError()) {
        String e = Update.errorString();
        s_flashAuth = false;
        server.send(500, "text/plain", String("Flash failed: ") + e);
        return;
    }
    s_flashAuth = false;
    if (prefsDirty.load(std::memory_order_relaxed)) {
        if (flushPrefs())
            prefsDirty.store(false, std::memory_order_relaxed);
        else
            LOG_WARN("Flash: NVS pre-flush failed — settings may not persist");
    }
    server.send(200, "text/plain", "Flash complete — rebooting...");
    server.client().flush();
    server.client().stop();
    delay(100);
    ESP.restart();
}

void registerBasicHandlers() {
    server.on("/",          handleRoot);
    server.on("/state",     handleState);
    server.on("/setb",      handleSetB);
    server.on("/setc",      handleSetC);
    server.on("/setco",     handleSetCo);
    server.on("/setsp",     handleSetSp);
    server.on("/setbl",     handleSetBl);
    server.on("/settheme",  handleSetTheme);
    server.on("/reset",     handleReset);
    server.on("/info",      handleInfo);
    server.on("/debug",     handleDebug);
    server.on("/log",       handleLog);
    server.on("/resetwifi", handleResetWifi);
    server.on("/flash", HTTP_GET,  handleFlashPage);
    server.on("/flash", HTTP_POST, handleFlashDone, handleFlashUpload);
}

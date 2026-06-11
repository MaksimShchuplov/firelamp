#include <Arduino.h>
#include <WiFi.h>
#include "globals.h"
#include "net_helpers.h"
#include "page.h"
#include "mqtt.h"

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
    // Require the string to be exactly [-]digit+ so that inputs like
    // '12abc', '--5', or '0x10' are rejected rather than silently coerced
    // by toInt()'s first-valid-prefix behaviour.
    unsigned i = (s[0] == '-') ? 1 : 0;
    if (i >= s.length()) return false;  // lone '-' with no digits
    for (; i < s.length(); i++)
        if (!isdigit((unsigned char)s[i])) return false;
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
    mqttPublishState();
}

void truncateUtf8(String &s, int maxChars) {
    if ((int)s.length() <= maxChars) return;
    int bytePos = 0, chars = 0;
    while (bytePos < (int)s.length() && chars < maxChars) {
        uint8_t c = (uint8_t)s[bytePos];
        int seqLen = (c < 0x80) ? 1 : (c < 0xC0) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        if (bytePos + seqLen > (int)s.length()) break;
        bytePos += seqLen; chars++;
    }
    s = s.substring(0, bytePos);
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

// Shared body for setc/setco/setsp/setbl/settheme: validate → change-check → side-effect → dirty → respond.
static bool applySimpleParam(int lo, int hi, std::atomic<uint8_t> &param, void (*onChanged)() = nullptr) {
    if (!isWebRequest()) return false;
    int v;
    if (!parseIntArg("v", lo, hi, v)) { sendInvalid(); return false; }
    if (param.load(std::memory_order_relaxed) != (uint8_t)v) {
        param.store((uint8_t)v, std::memory_order_relaxed);
        if (onChanged) onChanged();
        markDirty();
    }
    updatePowerCalc(); sendVal();
    return true;
}

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
    if (!parseIntArg("v", BRIGHT_MIN, BRIGHT_MAX, v)) { sendInvalid(); return; }
    setBright(v); updatePowerCalc();
    sendVal();
}

static void handleSetC()     { applySimpleParam(CONTRAST_MIN, CONTRAST_MAX,   uiContrast, buildHeatPalette); }
static void handleSetCo()    { applySimpleParam(COOLING_MIN,  COOLING_MAX,    uiCooling,  recalcCooling); }
static void handleSetSp()    { applySimpleParam(SPARKING_MIN, SPARKING_MAX,   uiSparking); }
static void handleSetBl()    { applySimpleParam(BLEND_MIN,    BLEND_MAX,      uiBlend); }
static void handleSetTheme() { applySimpleParam(0,            THEME_COUNT-1,  uiTheme,    buildHeatPalette); }

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
}

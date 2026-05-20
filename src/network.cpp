#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <HTTPUpdate.h>
#include <WiFiClientSecure.h>
#include <Update.h>
#include "globals.h"
#include "page.h"

// =============================================================================
//  HELPERS
// =============================================================================

void sendVal() {
    char j[96];
    snprintf(j, sizeof(j),
             "{\"b\":%d,\"c\":%d,\"co\":%d,\"sp\":%d,\"w\":%.1f,\"bl\":%d,\"th\":%d,\"upd\":%d}",
             (int)uiBright, (int)uiContrast, (int)uiCooling, (int)uiSparking,
             (float)currentPowerMw / 1000.0f, (int)uiBlend, (int)uiTheme, updatePending ? 1 : 0);
    server.send(200, "application/json", j);
}

// Fetches version.json from GitHub. Caches result for 60 s. Returns false on network error.
// TLS note: setInsecure() skips CA verification; firmware integrity is instead guaranteed
// by the MD5 hash embedded in version.json (checked by Update.setMD5 before flashing).
// To enable full TLS verification, replace setInsecure() with setCACert() pointing to
// GitHub's root CA (DigiCert Global Root CA / G2) stored in a separate header.
static bool fetchVersionInfo(String &ver, String &md5, uint32_t &buildN) {
    static String   s_ver, s_md5;
    static uint32_t s_buildN = 0, s_at = 0;
    static std::atomic<bool> s_busy{false};
    if (s_ver.length() > 0 && millis() - s_at < VERSION_CACHE_MS) {
        ver = s_ver; md5 = s_md5; buildN = s_buildN; return true;
    }
    // Guard against re-entrant calls from autoUpdateCheck task and HTTP handlers
    // running on the same core under preemptive FreeRTOS scheduling.
    // compare_exchange_strong atomically checks-and-sets, avoiding the TOCTOU
    // race that a separate load + store would have under preemptive scheduling.
    bool expected = false;
    if (!s_busy.compare_exchange_strong(expected, true)) return false;
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.begin(client, VERSION_URL);
    int code = http.GET();
    if (code != 200) {
        LOG_WARN("version fetch returned HTTP %d", code);
        http.end(); s_busy = false; return false;
    }
    String body = http.getString();
    http.end();
    auto extractStr = [&](const char *key) -> String {
        String s = String("\"") + key + "\":\"";
        int i = body.indexOf(s); if (i < 0) return "";
        i += s.length();
        int e = body.indexOf("\"", i);
        return (e > i) ? body.substring(i, e) : "";
    };
    auto extractNum = [&](const char *key) -> uint32_t {
        String s = String("\"") + key + "\":";
        int i = body.indexOf(s); if (i < 0) return 0;
        i += s.length();
        int e = i;
        while (e < (int)body.length() && isdigit((unsigned char)body[e])) e++;
        return (e > i) ? (uint32_t)body.substring(i, e).toInt() : 0;
    };
    ver    = extractStr("version");
    md5    = extractStr("md5");
    buildN = extractNum("build_n");
    if (ver.length() == 0) { LOG_WARN("version.json parse failed"); s_busy = false; return false; }
    s_ver = ver; s_md5 = md5; s_buildN = buildN; s_at = millis();
    s_busy = false;
    return true;
}

// =============================================================================
//  HTTP HANDLERS
// =============================================================================

static String jsonEscape(const String &s) {
    String r;
    r.reserve(s.length() + 8);
    for (unsigned i = 0; i < s.length(); i++) {
        char c = s[i];
        if      (c == '"')          r += "\\\"";
        else if (c == '\\')         r += "\\\\";
        else if (c == '\n')         r += "\\n";
        else if (c == '\r')         r += "\\r";
        else if (c == '\t')         r += "\\t";
        else if ((uint8_t)c < 0x20) { /* skip other control characters */ }
        else                        r += c;
    }
    return r;
}

// Rejects requests without our custom header — blocks cross-origin CSRF attempts.
// Browser CORS pre-flight fails for cross-origin requests that set custom headers,
// so any request that reaches this check with the header set came from our own page.
static bool isWebRequest() {
    if (server.header("X-Requested-With") != "firelamp") {
        server.send(403, "text/plain", "Forbidden");
        return false;
    }
    return true;
}

// Writes all six UI parameters to NVS. Returns true if all writes succeeded.
// Uses the always-open global `prefs` handle (opened in startNetwork).
static bool flushPrefs() {
    bool ok = true;
    ok &= (prefs.putUChar("bright2",  uiBright)  == 1);
    ok &= (prefs.putUChar("contrast", uiContrast) == 1);
    ok &= (prefs.putUChar("cooling",  uiCooling)  == 1);
    ok &= (prefs.putUChar("sparking", uiSparking) == 1);
    ok &= (prefs.putUChar("blend",    uiBlend)    == 1);
    ok &= (prefs.putUChar("theme",    uiTheme)    == 1);
    return ok;
}

// Parses a bounded integer from a query parameter. Returns false and sends 400
// if the argument is absent or outside [lo, hi].
static bool parseIntArg(const char *name, int lo, int hi, int &out) {
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
    if (parseIntArg("v", 0, 100, v)) { setBright(v); updatePowerCalc(); }
    sendVal();
}

static void handleSetC() {
    if (!isWebRequest()) return;
    int v;
    if (parseIntArg("v", 0, 100, v)) {
        if (uiContrast != (uint8_t)v) {
            uiContrast = (uint8_t)v;
            buildHeatPalette();
            markDirty();
        }
        updatePowerCalc();
    }
    sendVal();
}

static void handleSetCo() {
    if (!isWebRequest()) return;
    int v;
    if (parseIntArg("v", 20, 150, v)) {
        uiCooling  = (uint8_t)v;
        markDirty();
        recalcCooling(); updatePowerCalc();
    }
    sendVal();
}

static void handleSetSp() {
    if (!isWebRequest()) return;
    int v;
    if (parseIntArg("v", 0, 255, v)) {
        uiSparking = (uint8_t)v;
        markDirty();
        updatePowerCalc();
    }
    sendVal();
}

// Suffixes, defaults, and live-value pointers for the 6 UI parameters per preset slot.
// kPS / kPDef / kPPtr order must stay in sync.
static const char * const kPS[]  = {"b","c","co","sp","bl","th"};
static const uint8_t      kPDef[]= {BRIGHT_DEFAULT, CONTRAST_DEFAULT, COOLING_DEFAULT,
                                     SPARKING_DEFAULT, BLEND_DEFAULT, THEME_DEFAULT};
static std::atomic<uint8_t> * const kPPtr[] = {
    &uiBright, &uiContrast, &uiCooling, &uiSparking, &uiBlend, &uiTheme
};

static void handleGetPresets() {
    // buf fits worst case: 8 slots × ~88 chars + delimiters ≈ 650 bytes.
    // 1024 gives 1.5× headroom for escaped names and future fields.
    char buf[1024];
    int  len = 0;
    // gpcat / gpfmt: safe helpers that clamp len so direct writes and
    // snprintf calls can never escape the buffer regardless of data size.
#define GPCAT(c) do { if (len < (int)sizeof(buf) - 1) buf[len++] = (c); } while (0)
#define GPFMT(...) do { \
        int _w = snprintf(buf + len, (int)sizeof(buf) - len, __VA_ARGS__); \
        if (_w > 0) len += (_w < (int)sizeof(buf) - len) ? _w : (int)sizeof(buf) - len - 1; \
    } while (0)
    Preferences p;
    p.begin("presets", true);
    GPCAT('[');
    for (int i = 0; i < PRESET_COUNT; i++) {
        char k[6];
        if (i > 0) GPCAT(',');
        snprintf(k, sizeof k, "p%dn", i);
        String nm = p.getString(k, "");
        // Escape for JSON — names may contain backslashes or quotes from old firmware
        nm.replace("\\", "\\\\");
        nm.replace("\"", "\\\"");
        GPFMT("{\"slot\":%d,\"name\":\"%s\"", i, nm.c_str());
        if (nm.length() > 0) {
            for (int j = 0; j < 6; j++) {
                snprintf(k, sizeof k, "p%d%s", i, kPS[j]);
                GPFMT(",\"%s\":%d", kPS[j], p.getUChar(k, kPDef[j]));
            }
        }
        GPCAT('}');
    }
    p.end();
    GPCAT(']');
    buf[len] = '\0';
#undef GPCAT
#undef GPFMT
    server.send(200, "application/json", buf);
}

static void handleSavePreset() {
    if (!isWebRequest()) return;
    if (!server.hasArg("name")) {
        server.send(400, "application/json", "{\"error\":\"missing params\"}"); return;
    }
    int slot;
    if (!parseIntArg("slot", 0, PRESET_COUNT - 1, slot)) {
        server.send(400, "application/json", "{\"error\":\"invalid slot\"}"); return;
    }
    String name = server.arg("name");
    name.trim();
    if (name.length() == 0) { server.send(400, "application/json", "{\"error\":\"empty name\"}"); return; }
    name.replace("\\", "");    // strip backslashes so stored names are always JSON-safe
    name.replace("\"", "'");   // replace double-quotes before truncating to avoid split surrogates
    if (name.length() > PRESET_NAME_MAX_LEN) name = name.substring(0, PRESET_NAME_MAX_LEN);
    Preferences p;
    p.begin("presets", false);
    char k[6];
    snprintf(k, sizeof k, "p%dn", slot); p.putString(k, name);
    for (int j = 0; j < 6; j++) {
        snprintf(k, sizeof k, "p%d%s", slot, kPS[j]);
        p.putUChar(k, *kPPtr[j]);
    }
    p.end();
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleLoadPreset() {
    if (!isWebRequest()) return;
    int slot;
    if (!parseIntArg("slot", 0, PRESET_COUNT - 1, slot)) {
        server.send(400, "application/json", "{\"error\":\"invalid slot\"}"); return;
    }
    Preferences p;
    p.begin("presets", true);
    char k[6];
    snprintf(k, sizeof k, "p%dn", slot);
    String name = p.getString(k, "");
    if (name.length() == 0) {
        p.end(); server.send(404, "application/json", "{\"error\":\"empty slot\"}"); return;
    }
    for (int j = 0; j < 6; j++) {
        snprintf(k, sizeof k, "p%d%s", slot, kPS[j]);
        *kPPtr[j] = p.getUChar(k, kPDef[j]);
    }
    p.end();
    buildHeatPalette(); recalcCooling(); updatePowerCalc();
    markDirty();
    sendVal();
}

static void handleSetBl() {
    if (!isWebRequest()) return;
    int v;
    if (parseIntArg("v", 0, 255, v)) {
        uiBlend = (uint8_t)v;
        markDirty();
    }
    sendVal();
}

static void handleSetTheme() {
    if (!isWebRequest()) return;
    int v;
    if (parseIntArg("v", 0, THEME_COUNT - 1, v)) {
        if (uiTheme != (uint8_t)v) {
            uiTheme = (uint8_t)v;
            buildHeatPalette();
            markDirty();
        }
    }
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

static void handleCheckUpdate() {
    String ver, md5;
    uint32_t buildN;
    if (!fetchVersionInfo(ver, md5, buildN)) {
        server.send(503, "application/json", "{\"error\":\"fetch_failed\"}"); return;
    }
    // Compare monotonic build numbers when available (build_n > 0 means a CI build).
    // Falls back to SHA inequality for firmware flashed locally (BUILD_N == 0).
    bool avail = (buildN > 0 && BUILD_N > 0) ? (buildN > BUILD_N)
                                              : (ver != String(FIRMWARE_VERSION));
    server.send(200, "application/json",
        "{\"current\":\"" FIRMWARE_VERSION "\",\"latest\":\"" + jsonEscape(ver) +
        "\",\"update_available\":" + (avail ? "true" : "false") +
        ",\"date\":\"" __DATE__ " " __TIME__ "\"}");
}

static void handleUpdate() {
    if (!isWebRequest()) return;
    String ver, md5;
    uint32_t buildN;
    if (!fetchVersionInfo(ver, md5, buildN) || md5.length() != 32) {
        server.send(503, "application/json", "{\"error\":\"fetch_failed\"}");
        return;
    }
    // Flush any pending NVS writes before rebooting so no settings are lost.
    if (prefsDirty) {
        if (!flushPrefs())
            LOG_WARN("OTA pre-flush: NVS write failed — settings may not persist after update");
        prefsDirty = false;
    }
    Update.setMD5(md5.c_str());
    server.send(200, "text/plain", "Update starting...");
    server.client().stop();
    WiFiClientSecure client; client.setInsecure();
    httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    LOG_INFO("OTA → %s  md5: %s", ver.c_str(), md5.c_str());
    t_httpUpdate_return r = httpUpdate.update(client, FIRMWARE_URL);
    if (r == HTTP_UPDATE_FAILED)
        LOG_ERROR("OTA failed (%d): %s",
                  httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
}

static void handleInfo() {
    char j[512];
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
    char j[640];
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

static void handleDeletePreset() {
    if (!isWebRequest()) return;
    int slot;
    if (!parseIntArg("slot", 0, PRESET_COUNT - 1, slot)) {
        server.send(400, "application/json", "{\"error\":\"invalid slot\"}"); return;
    }
    Preferences p;
    p.begin("presets", false);
    char k[6];
    snprintf(k, sizeof k, "p%dn", slot); p.remove(k);
    for (int j = 0; j < 6; j++) {
        snprintf(k, sizeof k, "p%d%s", slot, kPS[j]); p.remove(k);
    }
    p.end();
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleResetWifi() {
    if (!isWebRequest()) return;
    server.send(200, "text/plain", "WiFi cleared — rebooting into setup mode...");
    server.client().stop();
    delay(200);
    wm.resetSettings();
    ESP.restart();
}

static void handleSetGeminiKey() {
    if (!isWebRequest()) return;
    const String &k = server.arg("key");
    if (k.length() == 0 || k.length() > 64) {
        server.send(400, "application/json", "{\"error\":\"invalid_key\"}"); return;
    }
    Preferences p;
    p.begin("gemini", false);
    p.putString("key", k);
    p.end();
    server.send(200, "application/json", "{\"ok\":true}");
}

static void handleGeminiKey() {
    Preferences p;
    p.begin("gemini", true);
    bool set = p.isKey("key") && p.getString("key", "").length() > 0;
    p.end();
    server.send(200, "application/json", set ? "{\"set\":true}" : "{\"set\":false}");
}

static void handleSurprise() {
    if (!isWebRequest()) return;
    Preferences gp;
    gp.begin("gemini", true);
    String apiKey = gp.getString("key", "");
    gp.end();
    if (apiKey.length() == 0) {
        server.send(400, "application/json", "{\"error\":\"no_key\"}"); return;
    }

    static const char * const kScenes[] = {
        "midnight thunderstorm", "arctic tundra", "volcanic eruption", "deep ocean abyss",
        "northern lights aurora", "dying campfire", "neon city rain", "solar flare",
        "haunted forest", "candlelit cathedral", "desert mirage", "lava field at dusk",
        "bioluminescent cave", "blizzard whiteout", "ember meditation"
    };
    constexpr uint8_t kScenesCount = sizeof(kScenes) / sizeof(kScenes[0]);
    const char *scene = kScenes[random8(kScenesCount)];

    static const char * const kThemeNames[] = {"Fire", "Ember", "Plasma", "Ice"};
    char curState[128];  // max output ~95 bytes ("...th=3(Plasma). Make something strikingly different.")
    snprintf(curState, sizeof(curState),
             "Currently: b=%d c=%d co=%d sp=%d bl=%d th=%d(%s). Make something strikingly different.",
             (int)uiBright, (int)uiContrast, (int)uiCooling,
             (int)uiSparking, (int)uiBlend, (int)uiTheme,
             kThemeNames[(int)uiTheme < 4 ? (int)uiTheme : 0]);

    static const char kPromptBase[] =
        "You design fire effects for an 800-LED cylinder lamp (20 col \xc3\x97 40 rows, WS2812B). "
        "The LED cylinder is 125 mm diameter 1260 mm tall, inside a frosted glass globe 190 mm wide "
        "1380 mm tall \xe2\x80\x94 like a giant floor lamp. Frosted glass diffuses light beautifully.\n"
        "Parameters:\n"
        "b = brightness 0-100 (0=off, 100=full; gamma 2.2 curve)\n"
        "c = palette contrast 0-100 (0=yellows+whites, 50=warm balanced, 100=deep reds only)\n"
        "co = cooling 20-150 (20=very tall flames, 45=tall, 105=medium height, 150=quick embers)\n"
        "sp = sparking 0-255 (0=calm smoldering, 36=steady flame, 160=active fire, 255=raging inferno)\n"
        "bl = temporal blend 0-255 (0=sharp flicker, 50=natural fire, 200=slow motion, 255=frozen glow; sweet spot 30-80)\n"
        "th = theme 0-3 (0=Fire red/orange/white, 1=Ember deep dark red, 2=Plasma purple/magenta/white, 3=Ice blue/cyan/white)\n"
        "Create an unusual, evocative effect inspired by the scene: ";

    String prompt = String(kPromptBase) + scene + ". " + curState + " Short name max 10 chars (fits small button). "
        "Respond ONLY with valid JSON, no markdown: "
        "{\"name\":\"...\",\"b\":N,\"c\":N,\"co\":N,\"sp\":N,\"bl\":N,\"th\":N}";

    String body =
        String("{\"contents\":[{\"parts\":[{\"text\":\"") + jsonEscape(prompt) +
        "\"}]}],\"generationConfig\":{\"temperature\":1.4,\"maxOutputTokens\":120,"
        "\"thinkingConfig\":{\"thinkingBudget\":0}}}";

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(GEMINI_TIMEOUT_MS);
    // Pass the key as Authorization header rather than a URL query parameter so
    // it does not appear in ESP-IDF HTTP debug logs or server-side access logs.
    http.begin(client, "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("Authorization", String("Bearer ") + apiKey);
    int code = http.POST(body);
    if (code <= 0) {
        http.end(); server.send(503, "application/json", "{\"error\":\"fetch_failed\"}"); return;
    }
    if (code == 429) {
        http.end(); server.send(429, "application/json", "{\"error\":\"rate_limit\"}"); return;
    }
    if (code == 401 || code == 403) {
        http.end(); server.send(403, "application/json", "{\"error\":\"bad_key\"}"); return;
    }
    if (code != 200) {
        http.end(); server.send(503, "application/json", "{\"error\":\"fetch_failed\"}"); return;
    }
    String resp = http.getString();
    http.end();

    // Find the last "text" part that contains '{' — the actual model output.
    // Scanning inside each string value (not just the first char) handles markdown-wrapped
    // responses like ```json\n{...}```. Taking the last match means thinking parts
    // (which precede the real answer) are skipped even when they contain '{'.
    int ti = -1;
    for (int p = 0; ; ) {
        p = resp.indexOf("\"text\":", p);
        if (p < 0) break;
        int s = p + 7;
        while (s < (int)resp.length() && resp[s] == ' ') s++;
        if (s >= (int)resp.length() || resp[s] != '"') { p++; continue; }
        s++;  // skip opening quote
        bool inEsc = false;
        for (int j = s; j < (int)resp.length(); j++) {
            char c = resp[j];
            if (inEsc)     { inEsc = false; continue; }
            if (c == '\\') { inEsc = true;  continue; }
            if (c == '"')  break;   // closing quote — no { in this part
            if (c == '{')  { ti = j; break; }  // record, keep scanning for later parts
        }
        p++;
    }
    if (ti < 0) {
        LOG_WARN("Gemini: no \"text\" field found. HTTP %d, body len %d", code, resp.length());
        server.send(503, "application/json", "{\"error\":\"parse_failed\"}"); return;
    }
    String inner;
    inner.reserve(128);
    {
        bool esc = false, started = false;
        int depth = 0;
        for (int i = ti; i < (int)resp.length(); i++) {
            char c = resp[i];
            if (esc) {
                if (started) {
                    if      (c == '"')  inner += '"';
                    else if (c == '\\') inner += '\\';
                    else                inner += c;
                }
                esc = false; continue;
            }
            if (c == '\\') { esc = true; continue; }
            if (!started) { if (c == '{') { started = true; depth = 1; inner += c; } continue; }
            inner += c;
            if      (c == '{') depth++;
            else if (c == '}') { if (--depth == 0) break; }
        }
    }
    if (inner.length() < 10) {
        LOG_WARN("Gemini: inner JSON too short (%d chars). Partial resp: %.200s",
                 inner.length(), resp.c_str());
        server.send(503, "application/json", "{\"error\":\"parse_failed\"}"); return;
    }

    // Both helpers skip optional whitespace after ':' so they work with
    // compact ("key":"val") and pretty-printed ("key": "val") inner JSON.
    auto exStr = [&](const char *fld) -> String {
        String s = String("\"") + fld + "\":";
        int i = inner.indexOf(s); if (i < 0) return "";
        i += s.length();
        while (i < (int)inner.length() && inner[i] == ' ') i++;
        if (i >= (int)inner.length() || inner[i] != '"') return "";
        i++;
        int e = inner.indexOf("\"", i);
        return (e > i) ? inner.substring(i, e) : "";
    };
    auto exNum = [&](const char *fld) -> int {
        String s = String("\"") + fld + "\":";
        int i = inner.indexOf(s); if (i < 0) return -1;
        i += s.length();
        while (i < (int)inner.length() && inner[i] == ' ') i++;
        int e = i;
        while (e < (int)inner.length() && (isdigit((unsigned char)inner[e]) || (e == i && inner[e] == '-'))) e++;
        return (e > i) ? inner.substring(i, e).toInt() : -1;
    };

    String name = exStr("name");
    int b  = exNum("b"),  cv = exNum("c"),  co = exNum("co");
    int sp = exNum("sp"), bl = exNum("bl"), th = exNum("th");
    if (name.length() == 0 || b < 0) {
        LOG_WARN("Gemini: missing fields in inner JSON: name=\"%s\" b=%d. inner: %.120s",
                 name.c_str(), b, inner.c_str());
        server.send(503, "application/json", "{\"error\":\"parse_failed\"}"); return;
    }

    if (b  >= 0) setBright(constrain(b,  0, 100));
    if (cv >= 0) { uiContrast = (uint8_t)constrain(cv, 0,   100); }
    if (co >= 0) { uiCooling  = (uint8_t)constrain(co, 20,  150); recalcCooling(); }
    if (sp >= 0) { uiSparking = (uint8_t)constrain(sp, 0,   255); }
    if (bl >= 0) { uiBlend    = (uint8_t)constrain(bl, 0,   255); }
    if (th >= 0) { uiTheme    = (uint8_t)constrain(th, 0,   THEME_COUNT - 1); }
    buildHeatPalette();
    updatePowerCalc();
    markDirty();

    if (name.length() > PRESET_NAME_MAX_LEN) name = name.substring(0, PRESET_NAME_MAX_LEN);
    char j[192];
    snprintf(j, sizeof(j),
             "{\"ok\":true,\"name\":\"%s\",\"b\":%d,\"c\":%d,\"co\":%d,"
             "\"sp\":%d,\"bl\":%d,\"th\":%d,\"w\":%.1f}",
             jsonEscape(name).c_str(),
             (int)uiBright, (int)uiContrast, (int)uiCooling,
             (int)uiSparking, (int)uiBlend, (int)uiTheme,
             (float)currentPowerMw / 1000.0f);
    server.send(200, "application/json", j);
}

// =============================================================================
//  STARTUP / SERVICE
// =============================================================================

static void autoUpdateCheck(void *) {
    vTaskDelay(pdMS_TO_TICKS(OTA_CHECK_DELAY_MS));
    String ver, md5;
    uint32_t buildN;
    if (fetchVersionInfo(ver, md5, buildN))
        updatePending = (buildN > 0 && BUILD_N > 0) ? (buildN > BUILD_N)
                                                    : (ver != String(FIRMWARE_VERSION));
    vTaskDelete(NULL);
}

void startNetwork() {
    prefs.begin("lamp", false);
    // "bright2": key was renamed from "bright" after gamma curve change to force
    // NVS re-read on existing devices; keep as-is to preserve settings in the field.
    uiBright   = prefs.getUChar("bright2",  BRIGHT_DEFAULT);
    uiContrast = prefs.getUChar("contrast", CONTRAST_DEFAULT);
    uiCooling  = prefs.getUChar("cooling",  COOLING_DEFAULT);
    uiSparking = prefs.getUChar("sparking", SPARKING_DEFAULT);
    uiBlend    = prefs.getUChar("blend",    BLEND_DEFAULT);
    uiTheme    = prefs.getUChar("theme",    THEME_DEFAULT);
    // Sanity-clamp in case NVS held out-of-range bytes from a corrupt write
    // or a downgrade from a future firmware with wider parameter ranges.
    uiBright   = (uint8_t)constrain((int)uiBright,   0,   100);
    uiContrast = (uint8_t)constrain((int)uiContrast, 0,   100);
    uiCooling  = (uint8_t)constrain((int)uiCooling,  20,  150);
    uiSparking = (uint8_t)constrain((int)uiSparking, 0,   255);
    uiBlend    = (uint8_t)constrain((int)uiBlend,    0,   255);
    uiTheme    = (uint8_t)constrain((int)uiTheme,    0,   THEME_COUNT - 1);
    applyBrightness();
    buildHeatPalette();
    recalcCooling();

    WiFi.setHostname(MDNS_NAME);
    WiFi.setSleep(false);

    // Immediately reset the retry timer on disconnect so serviceNetwork() picks
    // it up on the next 10 ms tick instead of waiting up to WIFI_RETRY_MS.
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
            wifiRetryAt = 0;
            LOG_WARN("WiFi disconnected (reason %d)", info.wifi_sta_disconnected.reason);
        } else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP) {
            LOG_INFO("WiFi connected — IP %s  RSSI %d dBm",
                     WiFi.localIP().toString().c_str(), WiFi.RSSI());
        }
    });

    wm.setConnectTimeout(10);
    wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_S);
    wm.setConnectRetries(2);

    if (wm.autoConnect(WIFI_PORTAL_SSID)) {
        LOG_INFO("UI: http://%s.local  |  http://%s", MDNS_NAME, WiFi.localIP().toString().c_str());
        if (MDNS.begin(MDNS_NAME)) {
            MDNS.addService("http", "tcp", 80);
        } else {
            LOG_WARN("mDNS start failed — firelamp.local will not resolve");
        }
        if (xTaskCreatePinnedToCore(autoUpdateCheck, "UpdChk", UPDCHK_STACK_BYTES, NULL, 1, NULL, 1) != pdPASS)
            LOG_WARN("autoUpdateCheck task not created — OTA badge disabled");
    } else {
        LOG_WARN("WiFi not configured — connect to \"%s\"", WIFI_PORTAL_SSID);
    }

    server.on("/",            handleRoot);
    server.on("/state",       []() { sendVal(); });
    server.on("/setb",        handleSetB);
    server.on("/setc",        handleSetC);
    server.on("/setco",       handleSetCo);
    server.on("/setsp",       handleSetSp);
    server.on("/setbl",       handleSetBl);
    server.on("/settheme",    handleSetTheme);
    server.on("/getpresets",  handleGetPresets);
    server.on("/savepreset",  handleSavePreset);
    server.on("/loadpreset",  handleLoadPreset);
    server.on("/reset",       handleReset);
    server.on("/checkupdate", handleCheckUpdate);
    server.on("/update",      handleUpdate);
    server.on("/info",        handleInfo);
    server.on("/debug",       handleDebug);
    server.on("/deletepreset",  handleDeletePreset);
    server.on("/resetwifi",     handleResetWifi);
    server.on("/setgeminikey",  HTTP_POST, handleSetGeminiKey);
    server.on("/geminikey",     handleGeminiKey);
    server.on("/surprise",      handleSurprise);
    server.onNotFound([]() { server.send(404, "text/plain", "404"); });
    static const char *hdrs[] = {"X-Requested-With"};
    server.collectHeaders(hdrs, 1);
    server.begin();
}

void serviceNetwork() {
    server.handleClient();

    if (prefsDirty && millis() - prefsTouch > NVS_COMMIT_DELAY_MS) {
        if (flushPrefs()) {
            prefsDirty = false;
        } else {
            LOG_ERROR("NVS write failed — will retry in %d ms", NVS_COMMIT_DELAY_MS);
            prefsTouch = millis();   // push the retry window
        }
    }

    if (WiFi.status() != WL_CONNECTED && millis() - wifiRetryAt > WIFI_RETRY_MS) {
        wifiRetryAt = millis();
        WiFi.reconnect();
    }
}

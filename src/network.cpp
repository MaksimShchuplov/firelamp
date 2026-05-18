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
    if (s_ver.length() > 0 && millis() - s_at < 60000) {
        ver = s_ver; md5 = s_md5; buildN = s_buildN; return true;
    }
    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.begin(client, VERSION_URL);
    int code = http.GET();
    if (code != 200) {
        LOG_WARN("version fetch returned HTTP %d", code);
        http.end(); return false;
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
    if (ver.length() == 0) { LOG_WARN("version.json parse failed"); return false; }
    s_ver = ver; s_md5 = md5; s_buildN = buildN; s_at = millis();
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
    server.sendHeader("Content-Security-Policy",
        "default-src 'self'; script-src 'self' 'unsafe-inline'; object-src 'none'; base-uri 'self'");
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
    int v;
    if (parseIntArg("v", 0, 100, v)) { setBright(v); updatePowerCalc(); }
    sendVal();
}

static void handleSetC() {
    int v;
    if (parseIntArg("v", 0, 100, v)) {
        if (uiContrast != (uint8_t)v) {
            uiContrast = (uint8_t)v;
            buildHeatPalette();
            prefsDirty = true; prefsTouch = millis();
        }
        updatePowerCalc();
    }
    sendVal();
}

static void handleSetCo() {
    int v;
    if (parseIntArg("v", 20, 150, v)) {
        uiCooling  = (uint8_t)v;
        prefsDirty = true; prefsTouch = millis();
        recalcCooling(); updatePowerCalc();
    }
    sendVal();
}

static void handleSetSp() {
    int v;
    if (parseIntArg("v", 0, 255, v)) {
        uiSparking = (uint8_t)v;
        prefsDirty = true; prefsTouch = millis();
        updatePowerCalc();
    }
    sendVal();
}

// Suffixes and defaults for the 6 UI parameters stored per preset slot.
// Order must match kPDef and kPPtr (declared locally where writes are needed).
static const char * const kPS[]  = {"b","c","co","sp","bl","th"};
static const uint8_t      kPDef[]= {BRIGHT_DEFAULT, CONTRAST_DEFAULT, COOLING_DEFAULT,
                                     SPARKING_DEFAULT, BLEND_DEFAULT, THEME_DEFAULT};

static void handleGetPresets() {
    Preferences p;
    p.begin("presets", true);
    char buf[900];
    int  len = 0;
    buf[len++] = '[';
    for (int i = 0; i < PRESET_COUNT; i++) {
        char k[6];
        if (i > 0) buf[len++] = ',';
        snprintf(k, sizeof k, "p%dn", i);
        String nm = p.getString(k, "");
        // Escape for JSON — names may contain backslashes or quotes from old firmware
        nm.replace("\\", "\\\\");
        nm.replace("\"", "\\\"");
        len += snprintf(buf + len, sizeof(buf) - len,
                        "{\"slot\":%d,\"name\":\"%s\"", i, nm.c_str());
        if (nm.length() > 0) {
            for (int j = 0; j < 6; j++) {
                snprintf(k, sizeof k, "p%d%s", i, kPS[j]);
                len += snprintf(buf + len, sizeof(buf) - len,
                                ",\"%s\":%d", kPS[j], p.getUChar(k, kPDef[j]));
            }
        }
        buf[len++] = '}';
    }
    p.end();
    buf[len++] = ']';
    buf[len]   = '\0';
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
    volatile uint8_t * const kPPtr[] = {&uiBright,&uiContrast,&uiCooling,&uiSparking,&uiBlend,&uiTheme};
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
    volatile uint8_t * const kPPtr[] = {&uiBright,&uiContrast,&uiCooling,&uiSparking,&uiBlend,&uiTheme};
    for (int j = 0; j < 6; j++) {
        snprintf(k, sizeof k, "p%d%s", slot, kPS[j]);
        *kPPtr[j] = p.getUChar(k, kPDef[j]);
    }
    p.end();
    buildHeatPalette(); recalcCooling(); updatePowerCalc();
    prefsDirty = true; prefsTouch = millis();
    sendVal();
}

static void handleSetBl() {
    int v;
    if (parseIntArg("v", 0, 255, v)) {
        uiBlend = (uint8_t)v;
        prefsDirty = true; prefsTouch = millis();
    }
    sendVal();
}

static void handleSetTheme() {
    int v;
    if (parseIntArg("v", 0, THEME_COUNT - 1, v)) {
        if (uiTheme != (uint8_t)v) {
            uiTheme = (uint8_t)v;
            buildHeatPalette();
            prefsDirty = true; prefsTouch = millis();
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
    prefsDirty = true; prefsTouch = millis();
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
        "{\"current\":\"" FIRMWARE_VERSION "\",\"latest\":\"" + ver +
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
    char j[384];
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
    char j[512];
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

// =============================================================================
//  STARTUP / SERVICE
// =============================================================================

static void autoUpdateCheck(void *) {
    vTaskDelay(pdMS_TO_TICKS(8000));
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
    uiBright   = constrain(uiBright,   0,   100);
    uiContrast = constrain(uiContrast, 0,   100);
    uiCooling  = constrain(uiCooling,  20,  150);
    uiSparking = constrain(uiSparking, 0,   255);
    uiBlend    = constrain(uiBlend,    0,   255);
    uiTheme    = constrain(uiTheme,    0,   THEME_COUNT - 1);
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
        if (MDNS.begin(MDNS_NAME)) MDNS.addService("http", "tcp", 80);
        if (xTaskCreate(autoUpdateCheck, "UpdChk", 8192, NULL, 1, NULL) != pdPASS)
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
    server.on("/deletepreset",handleDeletePreset);
    server.on("/resetwifi",   handleResetWifi);
    server.onNotFound([]() { server.send(404, "text/plain", "404"); });
    static const char *hdrs[] = {"X-Requested-With"};
    server.collectHeaders(hdrs, 1);
    server.begin();
}

void serviceNetwork() {
    server.handleClient();

    if (prefsDirty && millis() - prefsTouch > NVS_COMMIT_DELAY_MS) {
        bool ok = (prefs.putUChar("bright2",  uiBright)  == 1)
               && (prefs.putUChar("contrast", uiContrast) == 1)
               && (prefs.putUChar("cooling",  uiCooling)  == 1)
               && (prefs.putUChar("sparking", uiSparking) == 1)
               && (prefs.putUChar("blend",    uiBlend)    == 1)
               && (prefs.putUChar("theme",    uiTheme)    == 1);
        if (ok) {
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

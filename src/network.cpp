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
             (float)currentPowerW, (int)uiBlend, (int)uiTheme, updatePending ? 1 : 0);
    server.send(200, "application/json", j);
}

// Fetches version.json from GitHub. Caches result for 60 s. Returns false on network error.
static bool fetchVersionInfo(String &ver, String &md5) {
    static String  s_ver, s_md5;
    static uint32_t s_at = 0;
    if (s_ver.length() > 0 && millis() - s_at < 60000) {
        ver = s_ver; md5 = s_md5; return true;
    }
    WiFiClientSecure client;
    client.setInsecure();           // see note in config.h about CA pinning
    HTTPClient http;
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.begin(client, VERSION_URL);
    int code = http.GET();
    if (code != 200) { http.end(); return false; }
    String body = http.getString();
    http.end();
    auto extract = [&](const char *key) -> String {
        String s = String("\"") + key + "\":\"";
        int i = body.indexOf(s); if (i < 0) return "";
        i += s.length();
        int e = body.indexOf("\"", i);
        return (e > i) ? body.substring(i, e) : "";
    };
    ver = extract("version");
    md5 = extract("md5");
    if (ver.length() == 0) return false;
    s_ver = ver; s_md5 = md5; s_at = millis();
    return true;
}

// =============================================================================
//  HTTP HANDLERS
// =============================================================================

// Rejects requests without our custom header — blocks cross-origin CSRF attempts.
// Browser CORS pre-flight fails on unknown origins before the request reaches us.
static bool isWebRequest() {
    if (server.header("X-Requested-With") != "firelamp") {
        server.send(403, "text/plain", "Forbidden");
        return false;
    }
    return true;
}

static void handleRoot() {
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
    if (server.hasArg("v")) { setBright(server.arg("v").toInt()); updatePowerCalc(); }
    sendVal();
}

static void handleSetC() {
    if (server.hasArg("v")) {
        int v = constrain(server.arg("v").toInt(), 0, 100);
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
    if (server.hasArg("v")) {
        uiCooling  = (uint8_t)constrain(server.arg("v").toInt(), 20, 150);
        prefsDirty = true; prefsTouch = millis();
        recalcCooling(); updatePowerCalc();
    }
    sendVal();
}

static void handleSetSp() {
    if (server.hasArg("v")) {
        uiSparking = (uint8_t)constrain(server.arg("v").toInt(), 0, 255);
        prefsDirty = true; prefsTouch = millis();
        updatePowerCalc();
    }
    sendVal();
}

static void handleSetBl() {
    if (server.hasArg("v")) {
        uiBlend = (uint8_t)constrain(server.arg("v").toInt(), 0, 255);
        prefsDirty = true; prefsTouch = millis();
    }
    sendVal();
}

static void handleSetTheme() {
    if (server.hasArg("v")) {
        uint8_t t = (uint8_t)constrain(server.arg("v").toInt(), 0, 3);
        if (uiTheme != t) { uiTheme = t; buildHeatPalette(); prefsDirty = true; prefsTouch = millis(); }
    }
    sendVal();
}

static void handleReset() {
    if (!isWebRequest()) return;
    uiBright = BRIGHT_DEFAULT; uiContrast = CONTRAST_DEFAULT;
    uiCooling = COOLING_DEFAULT; uiSparking = SPARKING_DEFAULT;
    uiBlend = BLEND_DEFAULT; uiTheme = THEME_DEFAULT;
    applyBrightness(); buildHeatPalette(); recalcCooling();
    prefsDirty = true; prefsTouch = millis();
    updatePowerCalc(); sendVal();
}

static void handleCheckUpdate() {
    String ver, md5;
    if (!fetchVersionInfo(ver, md5)) {
        server.send(503, "application/json", "{\"error\":\"fetch_failed\"}"); return;
    }
    bool avail = (ver != String(FIRMWARE_VERSION));
    server.send(200, "application/json",
        "{\"current\":\"" FIRMWARE_VERSION "\",\"latest\":\"" + ver +
        "\",\"update_available\":" + (avail ? "true" : "false") +
        ",\"date\":\"" __DATE__ " " __TIME__ "\"}");
}

static void handleUpdate() {
    if (!isWebRequest()) return;
    String ver, md5;
    if (!fetchVersionInfo(ver, md5) || md5.length() != 32) {
        server.send(503, "application/json", "{\"error\":\"fetch_failed\"}");
        return;
    }
    Update.setMD5(md5.c_str());
    server.send(200, "text/plain", "Update starting...");
    server.client().stop();
    WiFiClientSecure client; client.setInsecure();
    httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    Serial.printf("OTA → %s  md5: %s\n", ver.c_str(), md5.c_str());
    t_httpUpdate_return r = httpUpdate.update(client, FIRMWARE_URL);
    if (r == HTTP_UPDATE_FAILED)
        Serial.printf("OTA failed (%d): %s\n",
                      httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
}

static void handleInfo() {
    char j[192];
    String ip = WiFi.localIP().toString();
    snprintf(j, sizeof(j),
             "{\"flash_mb\":%u,\"free_heap\":%u,\"ip\":\"%s\","
             "\"version\":\"" FIRMWARE_VERSION "\",\"build\":\"" __DATE__ " " __TIME__ "\"}",
             (unsigned)(ESP.getFlashChipSize() / (1024*1024)),
             (unsigned)ESP.getFreeHeap(), ip.c_str());
    server.send(200, "application/json", j);
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
    if (fetchVersionInfo(ver, md5))
        updatePending = (ver != String(FIRMWARE_VERSION));
    vTaskDelete(NULL);
}

void startNetwork() {
    prefs.begin("lamp", false);
    uiBright   = prefs.getUChar("bright2",  BRIGHT_DEFAULT);
    uiContrast = prefs.getUChar("contrast", CONTRAST_DEFAULT);
    uiCooling  = prefs.getUChar("cooling",  COOLING_DEFAULT);
    uiSparking = prefs.getUChar("sparking", SPARKING_DEFAULT);
    uiBlend    = prefs.getUChar("blend",    BLEND_DEFAULT);
    uiTheme    = prefs.getUChar("theme",    THEME_DEFAULT);
    applyBrightness();
    buildHeatPalette();
    recalcCooling();

    WiFi.setHostname(MDNS_NAME);
    WiFi.setSleep(false);
    wm.setConnectTimeout(10);
    wm.setConfigPortalTimeout(WIFI_PORTAL_TIMEOUT_S);
    wm.setConnectRetries(2);

    if (wm.autoConnect(WIFI_PORTAL_SSID)) {
        Serial.printf("UI: http://%s.local  |  http://%s\n",
                      MDNS_NAME, WiFi.localIP().toString().c_str());
        if (MDNS.begin(MDNS_NAME)) MDNS.addService("http", "tcp", 80);
        markBootSuccess();
        xTaskCreate(autoUpdateCheck, "UpdChk", 8192, NULL, 1, NULL);
    } else {
        Serial.printf("WiFi not configured — connect to \"%s\"\n", WIFI_PORTAL_SSID);
    }

    server.on("/",            handleRoot);
    server.on("/state",       []() { sendVal(); });
    server.on("/setb",        handleSetB);
    server.on("/setc",        handleSetC);
    server.on("/setco",       handleSetCo);
    server.on("/setsp",       handleSetSp);
    server.on("/setbl",       handleSetBl);
    server.on("/settheme",    handleSetTheme);
    server.on("/reset",       handleReset);
    server.on("/checkupdate", handleCheckUpdate);
    server.on("/update",      handleUpdate);
    server.on("/info",        handleInfo);
    server.on("/resetwifi",   handleResetWifi);
    server.onNotFound([]() { server.send(404, "text/plain", "404"); });
    static const char *hdrs[] = {"X-Requested-With"};
    server.collectHeaders(hdrs, 1);
    server.begin();
}

void serviceNetwork() {
    server.handleClient();

    if (prefsDirty && millis() - prefsTouch > NVS_COMMIT_DELAY_MS) {
        prefs.putUChar("bright2",  uiBright);
        prefs.putUChar("contrast", uiContrast);
        prefs.putUChar("cooling",  uiCooling);
        prefs.putUChar("sparking", uiSparking);
        prefs.putUChar("blend",    uiBlend);
        prefs.putUChar("theme",    uiTheme);
        prefsDirty = false;
    }

    if (WiFi.status() != WL_CONNECTED && millis() - wifiRetryAt > WIFI_RETRY_MS) {
        wifiRetryAt = millis();
        WiFi.reconnect();
    }
}

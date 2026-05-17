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
    char j[64];
    snprintf(j, sizeof(j), "{\"b\":%d,\"c\":%d,\"co\":%d,\"sp\":%d,\"w\":%.1f}",
             (int)uiBright, (int)uiContrast, (int)uiCooling, (int)uiSparking,
             (float)currentPowerW);
    server.send(200, "application/json", j);
}

// Fetches version.json from GitHub. Returns false on network error.
static bool fetchVersionInfo(String &ver, String &md5) {
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
    return ver.length() > 0;
}

// =============================================================================
//  HTTP HANDLERS
// =============================================================================

static void handleRoot() {
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200, "text/html", "");
    server.sendContent_P(PAGE);
    char init[96];
    snprintf(init, sizeof(init), "<script>pb(%d);pc(%d);pco(%d);psp(%d);</script>",
             (int)uiBright, (int)uiContrast, (int)uiCooling, (int)uiSparking);
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

static void handleReset() {
    uiBright = BRIGHT_DEFAULT; uiContrast = 50; uiCooling = 45; uiSparking = 36;
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
    String ver, md5;
    if (fetchVersionInfo(ver, md5) && md5.length() == 32)
        Update.setMD5(md5.c_str());
    server.send(200, "text/plain", "Update starting...");
    server.client().stop();
    WiFiClientSecure client; client.setInsecure();
    httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    Serial.printf("OTA → %s\n", ver.c_str());
    t_httpUpdate_return r = httpUpdate.update(client, FIRMWARE_URL);
    if (r == HTTP_UPDATE_FAILED)
        Serial.printf("OTA failed (%d): %s\n",
                      httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
}

static void handleInfo() {
    String j = "{\"flash_mb\":"; j += ESP.getFlashChipSize() / (1024*1024);
    j += ",\"free_heap\":";      j += ESP.getFreeHeap();
    j += ",\"ip\":\"";           j += WiFi.localIP().toString();
    j += "\",\"version\":\"" FIRMWARE_VERSION "\",\"build\":\"" __DATE__ " " __TIME__ "\"}";
    server.send(200, "application/json", j);
}

static void handleResetWifi() {
    server.send(200, "text/plain", "WiFi cleared — rebooting into setup mode...");
    server.client().stop();
    delay(200);
    wm.resetSettings();
    ESP.restart();
}

// =============================================================================
//  STARTUP / SERVICE
// =============================================================================

void startNetwork() {
    prefs.begin("lamp", false);
    uiBright   = prefs.getUChar("bright2",  BRIGHT_DEFAULT);
    uiContrast = prefs.getUChar("contrast", 50);
    uiCooling  = prefs.getUChar("cooling",  45);
    uiSparking = prefs.getUChar("sparking", 36);
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
    } else {
        Serial.printf("WiFi not configured — connect to \"%s\"\n", WIFI_PORTAL_SSID);
    }

    server.on("/",            handleRoot);
    server.on("/state",       []() { sendVal(); });
    server.on("/setb",        handleSetB);
    server.on("/setc",        handleSetC);
    server.on("/setco",       handleSetCo);
    server.on("/setsp",       handleSetSp);
    server.on("/reset",       handleReset);
    server.on("/checkupdate", handleCheckUpdate);
    server.on("/update",      handleUpdate);
    server.on("/info",        handleInfo);
    server.on("/resetwifi",   handleResetWifi);
    server.onNotFound([]() { server.send(404, "text/plain", "404"); });
    server.begin();
}

void serviceNetwork() {
    server.handleClient();

    if (prefsDirty && millis() - prefsTouch > NVS_COMMIT_DELAY_MS) {
        prefs.putUChar("bright2",  uiBright);
        prefs.putUChar("contrast", uiContrast);
        prefs.putUChar("cooling",  uiCooling);
        prefs.putUChar("sparking", uiSparking);
        prefsDirty = false;
    }

    if (WiFi.status() != WL_CONNECTED && millis() - wifiRetryAt > WIFI_RETRY_MS) {
        wifiRetryAt = millis();
        WiFi.reconnect();
    }
}

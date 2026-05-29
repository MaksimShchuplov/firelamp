#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "globals.h"
#include "net_helpers.h"

void startNetwork() {
    prefs.begin("lamp", false);
    // "bright2": key renamed from "bright" after gamma curve change to force
    // NVS re-read on existing devices; keep as-is to preserve field settings.
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

    // Reset retry timer on disconnect so serviceNetwork() picks it up immediately
    // rather than waiting up to WIFI_RETRY_MS.
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
        startAutoUpdateTask();
    } else {
        LOG_WARN("WiFi not configured — connect to \"%s\"", WIFI_PORTAL_SSID);
    }

    registerBasicHandlers();
    registerPresetHandlers();
    registerOtaHandlers();
    registerGeminiHandlers();
    server.onNotFound([]() { server.send(404, "text/plain", "404"); });
    static const char *hdrs[] = {"X-Requested-With"};
    server.collectHeaders(hdrs, 1);
    server.begin();
}

void serviceNetwork() {
    server.handleClient();

    if (prefsDirty.load(std::memory_order_relaxed) && millis() - prefsTouch.load(std::memory_order_relaxed) > NVS_COMMIT_DELAY_MS) {
        if (flushPrefs()) {
            prefsDirty.store(false, std::memory_order_relaxed);
        } else {
            LOG_ERROR("NVS write failed — will retry in %d ms", NVS_COMMIT_DELAY_MS);
            prefsTouch.store(millis(), std::memory_order_relaxed);
        }
    }

    if (WiFi.status() != WL_CONNECTED && millis() - wifiRetryAt > WIFI_RETRY_MS) {
        wifiRetryAt = millis();
        WiFi.reconnect();
    }
}

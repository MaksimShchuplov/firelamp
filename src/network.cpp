#include <Arduino.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include "globals.h"
#include "net_helpers.h"
#include "mqtt.h"

void loadSettings() {
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
    uiBright   = (uint8_t)constrain((int)uiBright,   BRIGHT_MIN,   BRIGHT_MAX);
    uiContrast = (uint8_t)constrain((int)uiContrast, CONTRAST_MIN, CONTRAST_MAX);
    uiCooling  = (uint8_t)constrain((int)uiCooling,  COOLING_MIN,  COOLING_MAX);
    uiSparking = (uint8_t)constrain((int)uiSparking, SPARKING_MIN, SPARKING_MAX);
    uiBlend    = (uint8_t)constrain((int)uiBlend,    BLEND_MIN,    BLEND_MAX);
    uiTheme    = (uint8_t)constrain((int)uiTheme,    0,            THEME_COUNT - 1);
    applyBrightness();
    buildHeatPalette();
    recalcCooling();
}

void startNetwork() {
    initMqtt();

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
    registerMqttHandlers();
    registerFlashHandlers();
    registerPwaHandlers();
    server.onNotFound([]() { server.send(404, "text/plain", "404"); });
    static const char *hdrs[] = {"X-Requested-With"};
    server.collectHeaders(hdrs, 1);
    server.begin();
}

void serviceNetwork() {
    server.handleClient();

    if (prefsDirty.load(std::memory_order_acquire) && millis() - prefsTouch.load(std::memory_order_relaxed) > NVS_COMMIT_DELAY_MS) {
        static uint8_t nvsFails = 0;
        if (flushPrefs()) {
            prefsDirty.store(false, std::memory_order_relaxed);
            nvsFails = 0;
        } else if (++nvsFails >= NVS_WRITE_MAX_RETRIES) {
            // Degraded flash: stop hammering it every NVS_COMMIT_DELAY_MS forever.
            // The next markDirty() re-arms a fresh round of retries.
            LOG_ERROR("NVS write failed %d times — giving up until next change", NVS_WRITE_MAX_RETRIES);
            prefsDirty.store(false, std::memory_order_relaxed);
            nvsFails = 0;
        } else {
            LOG_ERROR("NVS write failed — will retry in %d ms", NVS_COMMIT_DELAY_MS);
            prefsTouch.store(millis(), std::memory_order_relaxed);
        }
    }

    serviceMqtt();

    // Refresh per-row cooling caps periodically so they vary over time rather than
    // staying frozen at the single random sample taken on the last parameter change.
    static uint32_t lastCoolRecalc = 0;
    if (millis() - lastCoolRecalc > COOLING_RECALC_INTERVAL_MS) {
        recalcCooling();
        lastCoolRecalc = millis();
    }

    if (WiFi.status() != WL_CONNECTED && millis() - wifiRetryAt > WIFI_RETRY_MS) {
        wifiRetryAt = millis();
        WiFi.reconnect();
    }
}

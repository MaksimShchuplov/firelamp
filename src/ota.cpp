#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <Update.h>
#include "globals.h"
#include "net_helpers.h"

// Fetches version.json from GitHub. Caches result for VERSION_CACHE_MS. Returns false on error.
// TLS note: setInsecure() skips CA verification; firmware integrity is instead guaranteed
// by the MD5 hash embedded in version.json (checked by Update.setMD5 before flashing).
// To enable full TLS verification replace setInsecure() with setCACert() pointing to the
// GitHub root CA (DigiCert Global Root CA / G2) stored in a separate header.
static bool fetchVersionInfo(String &ver, String &md5, uint32_t &buildN) {
    static String            s_ver, s_md5;
    static uint32_t          s_buildN = 0, s_at = 0;
    static std::atomic<bool> s_busy{false};

    // Guard against re-entrant calls from autoUpdateCheck task and HTTP handlers
    // running on the same core under preemptive FreeRTOS scheduling.
    // compare_exchange_strong atomically checks-and-sets, avoiding the TOCTOU
    // race that a separate load + store would have under preemptive scheduling.
    // Cache read is also inside the lock so s_ver/s_md5/s_buildN are never
    // read while another task is writing them.
    bool expected = false;
    if (!s_busy.compare_exchange_strong(expected, true)) return false;

    // RAII: unconditionally release the lock on all exit paths.
    struct Guard { std::atomic<bool> &f; ~Guard() { f.store(false, std::memory_order_seq_cst); } } guard{s_busy};

    if (s_ver.length() > 0 && millis() - s_at < VERSION_CACHE_MS) {
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

static void handleCheckUpdate() {
    if (!isWebRequest()) return;
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

static bool isValidMd5(const String &s) {
    if (s.length() != 32) return false;
    for (unsigned i = 0; i < 32; i++) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return false;
    }
    return true;
}

static void handleUpdate() {
    if (!isWebRequest()) return;
    String ver, md5;
    uint32_t buildN;
    if (!fetchVersionInfo(ver, md5, buildN) || !isValidMd5(md5)) {
        server.send(503, "application/json", "{\"error\":\"fetch_failed\"}");
        return;
    }
    // Flush any pending NVS writes before rebooting so no settings are lost.
    if (prefsDirty) {
        if (flushPrefs())
            prefsDirty = false;
        else
            LOG_WARN("OTA pre-flush: NVS write failed — settings may not persist after update");
    }
    Update.setMD5(md5.c_str());
    server.send(200, "text/plain", "Update starting...");
    server.client().flush();
    server.client().stop();
    WiFiClientSecure client; client.setInsecure();
    httpUpdate.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    LOG_INFO("OTA → %s  md5: %s", ver.c_str(), md5.c_str());
    t_httpUpdate_return r = httpUpdate.update(client, FIRMWARE_URL);
    if (r == HTTP_UPDATE_FAILED)
        LOG_ERROR("OTA failed (%d): %s",
                  httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
}

static void autoUpdateCheck(void *) {
    vTaskDelay(pdMS_TO_TICKS(OTA_CHECK_DELAY_MS));
    String ver, md5;
    uint32_t buildN;
    if (fetchVersionInfo(ver, md5, buildN))
        updatePending = (buildN > 0 && BUILD_N > 0) ? (buildN > BUILD_N)
                                                    : (ver != String(FIRMWARE_VERSION));
    vTaskDelete(NULL);
}

void registerOtaHandlers() {
    server.on("/checkupdate", handleCheckUpdate);
    server.on("/update",      handleUpdate);
}

void startAutoUpdateTask() {
    if (xTaskCreatePinnedToCore(autoUpdateCheck, "UpdChk", UPDCHK_STACK_BYTES, NULL, 1, NULL, 1) != pdPASS)
        LOG_WARN("autoUpdateCheck task not created — OTA badge disabled");
}

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "globals.h"
#include "ota_utils.h"
#include "net_helpers.h"
#include "certs.h"

// Fetches version.json from GitHub. Caches result for VERSION_CACHE_MS. Returns false on error.
static bool fetchVersionInfo(String &ver, String &md5, uint32_t &buildN, bool *outBusy = nullptr,
                             bool force = false) {
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
    if (!s_busy.compare_exchange_strong(expected, true)) {
        if (outBusy) *outBusy = true;
        return false;
    }

    // RAII: unconditionally release the lock on all exit paths.
    struct Guard { std::atomic<bool> &f; ~Guard() { f.store(false, std::memory_order_seq_cst); } } guard{s_busy};

    // handleUpdate passes force: publishing `latest` replaces firmware.bin and
    // version.json in separate requests, so a cached md5 from before that window
    // can describe a different binary than the one about to be downloaded.
    if (!force && s_ver.length() > 0 && millis() - s_at < VERSION_CACHE_MS) {
        ver = s_ver; md5 = s_md5; buildN = s_buildN; return true;
    }

    WiFiClientSecure client;
    client.setCACertBundle(x509_crt_bundle_start);
    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    http.begin(client, VERSION_URL);
    int code = http.GET();
    if (code != 200) {
        LOG_WARN("version fetch returned HTTP %d", code);
        http.end(); return false;
    }
    String body = http.getString();
    http.end();
    {
        JsonDocument doc;
        if (deserializeJson(doc, body) != DeserializationError::Ok) {
            LOG_WARN("version.json parse failed");
            return false;
        }
        ver    = doc["version"] | "";
        md5    = doc["md5"] | "";
        buildN = (uint32_t)(doc["build_n"] | 0u);
    }
    if (ver.length() == 0) { LOG_WARN("version.json: missing version field"); return false; }
    s_ver = ver; s_md5 = md5; s_buildN = buildN; s_at = millis();
    return true;
}

static void handleCheckUpdate() {
    if (!isWebRequest()) return;
    String ver, md5;
    uint32_t buildN;
    bool busy = false;
    if (!fetchVersionInfo(ver, md5, buildN, &busy)) {
        server.send(503, "application/json", busy ? "{\"error\":\"busy\"}" : "{\"error\":\"fetch_failed\"}"); return;
    }
    bool avail = isNewerBuild(buildN, ver);
    server.send(200, "application/json",
        "{\"current\":\"" FIRMWARE_VERSION "\",\"latest\":\"" + jsonEscape(ver) +
        "\",\"update_available\":" + (avail ? "true" : "false") +
        ",\"date\":\"" __DATE__ " " __TIME__ "\"}");
}

static void handleUpdate() {
    if (!isWebRequest()) return;
    String ver, md5;
    uint32_t buildN;
    bool busy = false;
    if (!fetchVersionInfo(ver, md5, buildN, &busy, true) || !isValidMd5(md5)) {
        server.send(503, "application/json", busy ? "{\"error\":\"busy\"}" : "{\"error\":\"fetch_failed\"}");
        return;
    }
    // Flush any pending NVS writes before rebooting so no settings are lost.
    if (prefsDirty.load(std::memory_order_relaxed)) {
        if (flushPrefs())
            prefsDirty.store(false, std::memory_order_relaxed);
        else
            LOG_WARN("OTA pre-flush: NVS write failed — settings may not persist after update");
    }
    server.send(200, "text/plain", "Update starting...");
    server.client().flush();
    server.client().stop();

    otaProgress.store(0, std::memory_order_relaxed);
    isUpdating.store(true, std::memory_order_release);

    WiFiClientSecure dlClient;
    dlClient.setCACertBundle(x509_crt_bundle_start);
    HTTPClient http;
    http.setTimeout(OTA_DOWNLOAD_TIMEOUT_MS);
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    LOG_INFO("OTA → %s  md5: %s", ver.c_str(), md5.c_str());
    if (!http.begin(dlClient, FIRMWARE_URL)) {
        LOG_ERROR("OTA: http.begin failed");
        isUpdating.store(false, std::memory_order_relaxed);
        return;
    }
    int code = http.GET();
    if (code != 200) {
        LOG_ERROR("OTA firmware fetch: HTTP %d", code);
        http.end();
        isUpdating.store(false, std::memory_order_relaxed);
        return;
    }
    int fwSize = http.getSize();
    if (!Update.begin(fwSize > 0 ? (size_t)fwSize : UPDATE_SIZE_UNKNOWN)) {
        LOG_ERROR("OTA Update.begin: %s", Update.errorString());
        http.end();
        isUpdating.store(false, std::memory_order_relaxed);
        return;
    }
    // setMD5 must be called AFTER begin() — begin() resets the expected hash.
    Update.setMD5(md5.c_str());

    // Stream in 512-byte chunks so the LEDTask on Core 0 gets a tick between
    // flash writes and otaProgress stays fresh for the progress bar.
    WiFiClient *stream = http.getStreamPtr();
    size_t written = 0;
    uint8_t buf[512];
    while (Update.remaining() > 0) {
        size_t toRead = min(sizeof(buf), Update.remaining());
        size_t n = stream->readBytes(buf, toRead);
        if (n == 0) break;
        if (Update.write(buf, n) != n) break;
        written += n;
        if (fwSize > 0)
            otaProgress.store((uint8_t)(written * 100UL / (size_t)fwSize), std::memory_order_relaxed);
        else
            otaProgress.store((uint8_t)((millis() / 300) % 101), std::memory_order_relaxed);
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    http.end();

    // Keep isUpdating true on the success path so the LED bar stays full until the
    // reboot; only revert to fire if the flash actually failed (no reboot happens then).
    if (!Update.end()) {
        LOG_ERROR("OTA Update.end: %s", Update.errorString());
        isUpdating.store(false, std::memory_order_relaxed);
        return;
    }
    LOG_INFO("OTA written %u bytes — rebooting", (unsigned)written);
    blankStripForRestart();
    ESP.restart();
}

static void autoUpdateCheck(void *) {
    vTaskDelay(pdMS_TO_TICKS(OTA_CHECK_DELAY_MS));
    {
        String ver, md5;
        uint32_t buildN;
        if (fetchVersionInfo(ver, md5, buildN))
            updatePending = isNewerBuild(buildN, ver);
    }  // ver, md5 destructors run here before vTaskDelete
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

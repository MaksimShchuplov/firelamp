#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Update.h>
#include <ArduinoJson.h>
#include "globals.h"
#include "ota_utils.h"
#include "net_helpers.h"
#include "certs.h"

// GitHub's /releases/download/<tag>/<file> endpoint, which VERSION_URL and
// FIRMWARE_URL redirect through, took 10.5-11 s to answer its redirect in
// Nov 2026 and returned 504 for about one request in five; the old 8 s timeout
// therefore failed every check. The API answers in ~0.3 s, and an asset's API
// URL fetched with Accept: application/octet-stream redirects straight to the
// asset host. So both files are located through the API; the direct URLs stay
// only as a fallback for when the API refuses.
static bool resolveReleaseAssets(String &verUrl, String &fwUrl) {
    String body;
    {
        WiFiClientSecure client;
        client.setCACertBundle(x509_crt_bundle_start);
        HTTPClient http;
        http.setTimeout(HTTP_TIMEOUT_MS);
        if (!http.begin(client, RELEASE_API_URL)) return false;
        http.addHeader("Accept", "application/vnd.github+json");
        int code = http.GET();
        if (code != 200) {
            LOG_WARN("release API returned HTTP %d", code);
            http.end(); return false;
        }
        body = http.getString();
        http.end();
    }  // TLS session freed before the caller opens the next one
    JsonDocument filter;
    filter["assets"][0]["name"] = true;
    filter["assets"][0]["url"]  = true;
    JsonDocument doc;
    if (deserializeJson(doc, body, DeserializationOption::Filter(filter)) != DeserializationError::Ok) {
        LOG_WARN("release API: parse failed");
        return false;
    }
    verUrl = ""; fwUrl = "";
    for (JsonObject a : doc["assets"].as<JsonArray>()) {
        const char *name = a["name"] | "";
        const char *url  = a["url"]  | "";
        if      (strcmp(name, "version.json") == 0) verUrl = url;
        else if (strcmp(name, "firmware.bin") == 0) fwUrl  = url;
    }
    if (verUrl.length() == 0 || fwUrl.length() == 0) {
        LOG_WARN("release API: version.json or firmware.bin missing");
        return false;
    }
    return true;
}

// Opens a GET on a release asset. The Accept header makes an API asset URL
// redirect to the file instead of returning its metadata; the direct fallback
// URLs ignore it.
static int openAsset(HTTPClient &http, WiFiClientSecure &client, const String &url, uint32_t timeoutMs) {
    client.setCACertBundle(x509_crt_bundle_start);
    http.setTimeout(timeoutMs);
    http.setFollowRedirects(HTTPC_FORCE_FOLLOW_REDIRECTS);
    if (!http.begin(client, url)) return HTTPC_ERROR_CONNECTION_REFUSED;
    http.addHeader("Accept", "application/octet-stream");
    return http.GET();
}

// Fetches version.json from GitHub. Caches result for VERSION_CACHE_MS. Returns false on error.
// outFwUrl receives the firmware.bin URL resolved in the same lookup, so the md5
// and the binary come from one snapshot of the release.
static bool fetchVersionInfo(String &ver, String &md5, uint32_t &buildN, bool *outBusy = nullptr,
                             bool force = false, String *outFwUrl = nullptr) {
    static String            s_ver, s_md5, s_fwUrl;
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
        ver = s_ver; md5 = s_md5; buildN = s_buildN;
        if (outFwUrl) *outFwUrl = s_fwUrl;
        return true;
    }

    String verUrl, fwUrl;
    if (!resolveReleaseAssets(verUrl, fwUrl)) {
        verUrl = VERSION_URL; fwUrl = FIRMWARE_URL;
    }
    String body;
    {
        WiFiClientSecure client;
        HTTPClient http;
        int code = openAsset(http, client, verUrl, HTTP_TIMEOUT_MS);
        if (code != 200) {
            LOG_WARN("version fetch returned HTTP %d", code);
            http.end(); return false;
        }
        body = http.getString();
        http.end();
    }
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
    s_ver = ver; s_md5 = md5; s_buildN = buildN; s_fwUrl = fwUrl; s_at = millis();
    if (outFwUrl) *outFwUrl = fwUrl;
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
    String ver, md5, fwUrl;
    uint32_t buildN;
    bool busy = false;
    if (!fetchVersionInfo(ver, md5, buildN, &busy, true, &fwUrl) || !isValidMd5(md5)) {
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
    HTTPClient http;
    LOG_INFO("OTA → %s  md5: %s", ver.c_str(), md5.c_str());
    int code = openAsset(http, dlClient, fwUrl, OTA_DOWNLOAD_TIMEOUT_MS);
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

// One successful fetch per boot sets the badge; a failure (no DNS yet, GitHub
// unreachable, or the cache lock busy) is retried with quadrupling back-off
// instead of leaving the badge off until the next reboot.
static void autoUpdateCheck(void *) {
    uint32_t wait = OTA_CHECK_DELAY_MS;
    for (int attempt = 0; attempt < OTA_CHECK_MAX_TRIES; attempt++) {
        vTaskDelay(pdMS_TO_TICKS(wait));
        bool ok;
        {
            String ver, md5;
            uint32_t buildN;
            ok = fetchVersionInfo(ver, md5, buildN);
            if (ok) updatePending = isNewerBuild(buildN, ver);
        }  // ver, md5 destructors run here, before the next sleep / vTaskDelete
        if (ok) break;
        wait = min(wait * 4, (uint32_t)OTA_CHECK_RETRY_MAX_MS);
    }
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

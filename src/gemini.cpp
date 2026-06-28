#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include "globals.h"
#include "net_helpers.h"
#include "mqtt.h"
#include "certs.h"
#include "params.h"
#include <ArduinoJson.h>
static void handleSetGeminiKey() {
    if (!isWebRequest()) return;
    const String &k = server.arg("key");
    if (k.length() == 0 || k.length() > 64) {
        server.send(400, "application/json", "{\"error\":\"invalid_key\"}"); return;
    }
    for (int i = 0; i < (int)k.length(); i++) {
        // Reject control chars and non-ASCII; prevents HTTP header injection via \r\n.
        if ((uint8_t)k[i] < 0x21 || (uint8_t)k[i] > 0x7E) {
            server.send(400, "application/json", "{\"error\":\"invalid_key\"}"); return;
        }
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

// =============================================================================
//  Surprise Me — synchronous handler on Core 1; blocks up to GEMINI_TIMEOUT_MS.
// =============================================================================

static std::atomic<bool> s_surpriseBusy{false};

// Returns nullptr on success, or a short error-code string on failure.
static const char *surpriseBody(const String &apiKey, char *outName, size_t nameLen) {
#define FAIL(code) return (code)

    static const char * const kScenes[] = {
        "midnight thunderstorm", "arctic tundra", "volcanic eruption", "deep ocean abyss",
        "northern lights aurora", "dying campfire", "neon city rain", "solar flare",
        "haunted forest", "candlelit cathedral", "desert mirage", "lava field at dusk",
        "bioluminescent cave", "blizzard whiteout", "ember meditation"
    };
    constexpr uint8_t kScenesCount = sizeof(kScenes) / sizeof(kScenes[0]);
    const char *scene = kScenes[random8(kScenesCount)];

    static const char * const kThemeNames[] = {"Fire", "Ember", "Plasma", "Ice"};
    const int curTh = uiTheme;
    char curState[128];
    snprintf(curState, sizeof(curState),
             "Currently: b=%d c=%d co=%d sp=%d bl=%d th=%d(%s). Make something strikingly different.",
             (int)uiBright, (int)uiContrast, (int)uiCooling,
             (int)uiSparking, (int)uiBlend, curTh,
             kThemeNames[curTh < 4 ? curTh : 0]);

    static const char kPromptBase[] =
        "You design fire effects for a physical LED lamp. It is a 125mm diameter metal cylinder "
        "wrapped with a 20-column by 40-row matrix of 800 WS2812B LEDs.\n"
        "Parameters:\n"
        "b = brightness 0-100\n"
        "c = contrast 0-100 (20=yellowish, 50=warm, 90=deep red)\n"
        "co = cooling 20-150 (40=tall fire, 100=short sparks, avoid <30 to prevent solid columns)\n"
        "sp = sparking 0-255 (20=calm, 120=active, 255=raging)\n"
        "bl = blend 0-255 (20=slow, 50=natural, 200=sharp flicker)\n"
        "th = theme 0-3 (0=Fire, 1=Ember, 2=Plasma, 3=Ice)\n"
        "Create an unusual but aesthetically pleasing effect inspired by the scene: ";

    String prompt = String(kPromptBase) + scene + ". " + curState + " Short name max 10 chars. "
        "Respond ONLY with valid JSON: "
        "{\"name\":\"...\",\"b\":N,\"c\":N,\"co\":N,\"sp\":N,\"bl\":N,\"th\":N}";

    String body =
        String("{\"contents\":[{\"parts\":[{\"text\":\"") + jsonEscape(prompt) +
        "\"}]}],\"generationConfig\":{\"temperature\":1.1,\"maxOutputTokens\":120,"
        "\"thinkingConfig\":{\"thinkingBudget\":0},\"responseMimeType\":\"application/json\"}}";

    WiFiClientSecure client;
    client.setCACertBundle(x509_crt_bundle_start);
    HTTPClient http;
    http.setTimeout(GEMINI_TIMEOUT_MS);
    http.begin(client, "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-goog-api-key", apiKey);
    int code = http.POST(body);
    if (code <= 0) {
        LOG_ERROR("Gemini: HTTP POST failed, code: %d (%s)", code, http.errorToString(code).c_str());
        http.end();
        FAIL("timeout");
    }
    if (code == 429) { http.end(); FAIL("rate_limit"); }
    if (code == 401 || code == 403) { http.end(); FAIL("auth_error"); }
    if (code != 200) {
        String eb = http.getString(); http.end();
        LOG_WARN("Gemini: HTTP %d: %.130s", code, eb.c_str());
        if (code == 400 && eb.indexOf("API key") >= 0) FAIL("auth_error");
        FAIL("http_error");
    }
    String resp = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, resp);
    if (error) {
        LOG_WARN("Gemini: top-level JSON parse failed: %s", error.c_str());
        FAIL("parse_failed");
    }

    // Gemini API wraps the response in candidates[0].content.parts[0].text
    const char* innerText = doc["candidates"][0]["content"]["parts"][0]["text"];
    if (!innerText) {
        LOG_WARN("Gemini: missing candidates[0].content.parts[0].text");
        FAIL("parse_failed");
    }

    JsonDocument innerDoc;
    error = deserializeJson(innerDoc, innerText);
    if (error) {
        LOG_WARN("Gemini: inner JSON parse failed: %s. Text: %.50s", error.c_str(), innerText);
        FAIL("parse_failed");
    }

    String name = innerDoc["name"] | "";
    int b = innerDoc["b"] | -1;

    if (name.length() == 0 || b < 0) {
        LOG_WARN("Gemini: missing fields. name=\"%s\" b=%d", name.c_str(), b);
        FAIL("parse_failed");
    }

    // Brightness is autoJson=false (gamma path); apply it explicitly, then let
    // the registry clamp/apply c/co/sp/bl/th and fire their side-effects.
    setBright(constrain(b, BRIGHT_MIN, BRIGHT_MAX));
    applyJsonParams(innerDoc);
    buildHeatPalette();   // idempotent: early-outs if contrast/theme unchanged
    updatePowerCalc();
    markDirty();

    truncateUtf8(name, PRESET_NAME_MAX_LEN);

    String esc = jsonEscape(name);
    strncpy(outName, esc.c_str(), nameLen - 1);
    outName[nameLen - 1] = '\0';
    LOG_INFO("Gemini: applied \"%s\"", name.c_str());

#undef FAIL
    return nullptr;
}

static void handleSurprise() {
    if (!isWebRequest()) return;
    bool expected = false;
    if (!s_surpriseBusy.compare_exchange_strong(expected, true)) {
        server.send(429, "application/json", "{\"error\":\"rate_limit\"}"); return;
    }
    char apiKey[65] = {};
    {
        Preferences gp;
        gp.begin("gemini", true);
        String k = gp.getString("key", "");
        gp.end();
        strncpy(apiKey, k.c_str(), sizeof(apiKey) - 1);
    }
    if (apiKey[0] == '\0') {
        s_surpriseBusy.store(false, std::memory_order_release);
        server.send(400, "application/json", "{\"error\":\"no_key\"}"); return;
    }
    // Blocks up to GEMINI_TIMEOUT_MS while calling the Gemini API.
    // Core 0 (LEDs) runs uninterrupted. buildHeatPalette/recalcCooling are
    // Core-1-only and serialised by the HTTP handler context — same as before.
    char surpriseName[PRESET_NAME_MAX_LEN * 4 + 1] = {};
    const char *err = surpriseBody(apiKey, surpriseName, sizeof(surpriseName));
    if (err) {
        s_surpriseBusy.store(false, std::memory_order_release);
        int code = 400;
        if      (strcmp(err, "rate_limit")  == 0) code = 429;
        else if (strcmp(err, "timeout")     == 0) code = 504;
        else if (strcmp(err, "http_error")  == 0) code = 502;
        else if (strcmp(err, "parse_failed")== 0) code = 502;
        else if (strcmp(err, "auth_error")  == 0) code = 401;
        server.send(code, "application/json",
                    String("{\"error\":\"") + err + "\"}"); return;
    }
    // Build response while still holding s_surpriseBusy so formatState() reads
    // the same atomic values that surpriseBody() just wrote — releasing the gate
    // first would let a concurrent Core 1 preemption overwrite them before the
    // JSON snapshot is taken.
    // formatState worst case ~82 bytes; suffix ,\"name\":\"<60 chars>\"} needs up to 72 bytes.
    // Static assert catches future formatState growth before it silently truncates the response.
    static_assert(sizeof(char[256]) >= 96 + PRESET_NAME_MAX_LEN * 4 + 16,
                  "j[] too small for formatState output + name suffix");
    char j[256];
    formatState(j, sizeof(j));
    int slen = strlen(j);
    if (slen > 0 && j[slen - 1] == '}') {
        snprintf(j + slen - 1, sizeof(j) - (size_t)(slen - 1),
                 ",\"name\":\"%s\"}", surpriseName);
    }
    s_surpriseBusy.store(false, std::memory_order_release);
    server.send(200, "application/json", j);
    mqttPublishState();
}

void registerGeminiHandlers() {
    server.on("/setgeminikey", HTTP_POST, handleSetGeminiKey);
    server.on("/geminikey",    handleGeminiKey);
    server.on("/surprise",     handleSurprise);
}

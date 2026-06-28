#include <Arduino.h>
#include <Preferences.h>
#include "globals.h"
#include "net_helpers.h"
#include "params.h"

// Presets persist every parameter in the registry (b,c,co,sp,bl,th). Iterating
// PARAMS instead of five parallel arrays makes a key/range/pointer mismatch
// impossible — there is only one ordered list to keep consistent.

static void handleGetPresets() {
    // buf fits worst case: 8 slots × up to ~100 chars (name up to 60 bytes for 15 emoji
    // × 4 bytes each) + JSON framing ≈ 1280 bytes. 2048 gives comfortable headroom.
    char buf[2048];
    int  len = 0;
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
        // Strip control chars from names written by old firmware before JSON escaping.
        for (int ci = (int)nm.length() - 1; ci >= 0; ci--)
            if ((uint8_t)nm[ci] < 0x20) nm.remove(ci, 1);
        GPFMT("{\"slot\":%d,\"name\":\"%s\"", i, jsonEscape(nm).c_str());
        if (nm.length() > 0) {
            for (size_t j = 0; j < PARAM_COUNT; j++) {
                snprintf(k, sizeof k, "p%d%s", i, PARAMS[j].key);
                GPFMT(",\"%s\":%d", PARAMS[j].key, p.getUChar(k, PARAMS[j].def));
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
    name.replace("\\", "");    // strip backslashes so stored names are always JSON-safe
    name.replace("\"", "'");   // replace double-quotes before truncating to avoid split surrogates
    // Strip embedded control chars (0x01–0x1F); unescaped controls produce invalid JSON.
    for (int i = (int)name.length() - 1; i >= 0; i--)
        if ((uint8_t)name[i] < 0x20) name.remove(i, 1);
    if (name.length() == 0) { server.send(400, "application/json", "{\"error\":\"empty name\"}"); return; }
    truncateUtf8(name, PRESET_NAME_MAX_LEN);
    Preferences p;
    p.begin("presets", false);
    char k[6];
    snprintf(k, sizeof k, "p%dn", slot); p.putString(k, name);
    for (size_t j = 0; j < PARAM_COUNT; j++) {
        snprintf(k, sizeof k, "p%d%s", slot, PARAMS[j].key);
        // Explicit per-param values support the import path; default is the live value.
        int v;
        p.putUChar(k, parseIntArg(PARAMS[j].key, PARAMS[j].lo, PARAMS[j].hi, v)
                          ? (uint8_t)v : (uint8_t)*PARAMS[j].value);
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
    for (size_t j = 0; j < PARAM_COUNT; j++) {
        snprintf(k, sizeof k, "p%d%s", slot, PARAMS[j].key);
        *PARAMS[j].value = (uint8_t)constrain((int)p.getUChar(k, PARAMS[j].def),
                                              (int)PARAMS[j].lo, (int)PARAMS[j].hi);
    }
    p.end();
    buildHeatPalette(); recalcCooling(); updatePowerCalc();
    markDirty();
    sendVal();
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
    for (size_t j = 0; j < PARAM_COUNT; j++) {
        snprintf(k, sizeof k, "p%d%s", slot, PARAMS[j].key); p.remove(k);
    }
    p.end();
    server.send(200, "application/json", "{\"ok\":true}");
}

void registerPresetHandlers() {
    server.on("/getpresets",   handleGetPresets);
    server.on("/savepreset",   handleSavePreset);
    server.on("/loadpreset",   handleLoadPreset);
    server.on("/deletepreset", handleDeletePreset);
}

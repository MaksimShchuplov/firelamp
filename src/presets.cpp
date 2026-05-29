#include <Arduino.h>
#include <Preferences.h>
#include "globals.h"
#include "net_helpers.h"

// Suffixes, defaults, and live-value pointers for the 6 UI parameters per preset slot.
// kPS / kPDef / kPPtr order must stay in sync.
static const char * const kPS[]  = {"b","c","co","sp","bl","th"};
static const uint8_t      kPDef[]= {BRIGHT_DEFAULT, CONTRAST_DEFAULT, COOLING_DEFAULT,
                                     SPARKING_DEFAULT, BLEND_DEFAULT, THEME_DEFAULT};
static const uint8_t      kPLo[] = {0,   0,  20,  0,   0,   0};
static const uint8_t      kPHi[] = {100, 100, 150, 255, 255, THEME_COUNT - 1};
static std::atomic<uint8_t> * const kPPtr[] = {
    &uiBright, &uiContrast, &uiCooling, &uiSparking, &uiBlend, &uiTheme
};

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
        // Escape for JSON — names may contain backslashes or quotes from old firmware.
        nm.replace("\\", "\\\\");
        nm.replace("\"", "\\\"");
        GPFMT("{\"slot\":%d,\"name\":\"%s\"", i, nm.c_str());
        if (nm.length() > 0) {
            for (int j = 0; j < 6; j++) {
                snprintf(k, sizeof k, "p%d%s", i, kPS[j]);
                GPFMT(",\"%s\":%d", kPS[j], p.getUChar(k, kPDef[j]));
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
    // Truncate by Unicode codepoints, not bytes, to avoid splitting multibyte UTF-8 sequences
    // (e.g. Cyrillic = 2 bytes/char; naive byte-slice would store invalid UTF-8 in NVS).
    if (name.length() > (unsigned)PRESET_NAME_MAX_LEN) {
        int bytePos = 0, chars = 0;
        while (bytePos < (int)name.length() && chars < PRESET_NAME_MAX_LEN) {
            uint8_t c = (uint8_t)name[bytePos];
            // 0x80–0xBF are continuation bytes, never valid sequence starters —
            // treat them as 1-byte invalid units so a malformed input byte cannot
            // cause the loop to skip into the middle of a valid codepoint that follows.
            int seqLen = (c < 0x80) ? 1 : (c < 0xC0) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
            if (bytePos + seqLen > (int)name.length()) break;  // incomplete sequence at end
            bytePos += seqLen;
            chars++;
        }
        name = name.substring(0, bytePos);
    }
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
    for (int j = 0; j < 6; j++) {
        snprintf(k, sizeof k, "p%d%s", slot, kPS[j]);
        *kPPtr[j] = (uint8_t)constrain((int)p.getUChar(k, kPDef[j]), (int)kPLo[j], (int)kPHi[j]);
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
    for (int j = 0; j < 6; j++) {
        snprintf(k, sizeof k, "p%d%s", slot, kPS[j]); p.remove(k);
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

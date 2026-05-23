#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include "globals.h"
#include "net_helpers.h"

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

static void handleSurprise() {
    if (!isWebRequest()) return;
    Preferences gp;
    gp.begin("gemini", true);
    String apiKey = gp.getString("key", "");
    gp.end();
    if (apiKey.length() == 0) {
        server.send(400, "application/json", "{\"error\":\"no_key\"}"); return;
    }

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
    char curState[128];  // max output ~95 bytes ("...th=3(Plasma). Make something strikingly different.")
    snprintf(curState, sizeof(curState),
             "Currently: b=%d c=%d co=%d sp=%d bl=%d th=%d(%s). Make something strikingly different.",
             (int)uiBright, (int)uiContrast, (int)uiCooling,
             (int)uiSparking, (int)uiBlend, curTh,
             kThemeNames[curTh < 4 ? curTh : 0]);

    static const char kPromptBase[] =
        "You design fire effects for an 800-LED cylinder lamp (20 col \xc3\x97 40 rows, WS2812B). "
        "The LED cylinder is 125 mm diameter 1260 mm tall, inside a frosted glass globe 190 mm wide "
        "1380 mm tall \xe2\x80\x94 like a giant floor lamp. Frosted glass diffuses light beautifully.\n"
        "Parameters:\n"
        "b = brightness 0-100 (0=off, 100=full; gamma 2.2 curve)\n"
        "c = palette contrast 0-100 (0=yellows+whites, 50=warm balanced, 100=deep reds only)\n"
        "co = cooling 20-150 (20=very tall flames, 45=tall, 105=medium height, 150=quick embers)\n"
        "sp = sparking 0-255 (0=calm smoldering, 36=steady flame, 160=active fire, 255=raging inferno)\n"
        "bl = temporal blend 0-255 (0=sharp flicker, 50=natural fire, 200=slow motion, 255=frozen glow; sweet spot 30-80)\n"
        "th = theme 0-3 (0=Fire red/orange/white, 1=Ember deep dark red, 2=Plasma purple/magenta/white, 3=Ice blue/cyan/white)\n"
        "Create an unusual, evocative effect inspired by the scene: ";

    String prompt = String(kPromptBase) + scene + ". " + curState + " Short name max 10 chars (fits small button). "
        "Respond ONLY with valid JSON, no markdown: "
        "{\"name\":\"...\",\"b\":N,\"c\":N,\"co\":N,\"sp\":N,\"bl\":N,\"th\":N}";

    String body =
        String("{\"contents\":[{\"parts\":[{\"text\":\"") + jsonEscape(prompt) +
        "\"}]}],\"generationConfig\":{\"temperature\":1.4,\"maxOutputTokens\":120,"
        "\"thinkingConfig\":{\"thinkingBudget\":0}}}";

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.setTimeout(GEMINI_TIMEOUT_MS);
    // Pass the key via x-goog-api-key header rather than a URL query parameter so
    // it does not appear in ESP-IDF HTTP debug logs or server-side access logs.
    http.begin(client, "https://generativelanguage.googleapis.com/v1beta/models/gemini-2.5-flash:generateContent");
    http.addHeader("Content-Type", "application/json");
    http.addHeader("x-goog-api-key", apiKey);
    int code = http.POST(body);
    if (code <= 0) {
        http.end(); server.send(503, "application/json", "{\"error\":\"fetch_failed\"}"); return;
    }
    if (code == 429) {
        http.end(); server.send(429, "application/json", "{\"error\":\"rate_limit\"}"); return;
    }
    if (code == 401 || code == 403) {
        http.end(); server.send(403, "application/json", "{\"error\":\"bad_key\"}"); return;
    }
    if (code != 200) {
        http.end(); server.send(503, "application/json", "{\"error\":\"fetch_failed\"}"); return;
    }
    String resp = http.getString();
    http.end();

    // Find the last "text" part that contains '{' — the actual model output.
    // Scanning inside each string value handles markdown-wrapped responses like
    // ```json\n{...}```. Taking the last match skips thinking parts (which
    // precede the real answer) even when they contain '{'.
    int ti = -1;
    for (int p = 0; ; ) {
        p = resp.indexOf("\"text\":", p);
        if (p < 0) break;
        int s = p + 7;
        while (s < (int)resp.length() && resp[s] == ' ') s++;
        if (s >= (int)resp.length() || resp[s] != '"') { p++; continue; }
        s++;  // skip opening quote
        bool inEsc = false;
        for (int j = s; j < (int)resp.length(); j++) {
            char c = resp[j];
            if (inEsc)     { inEsc = false; continue; }
            if (c == '\\') { inEsc = true;  continue; }
            if (c == '"')  break;   // closing quote — no { in this part
            if (c == '{')  { ti = j; break; }
        }
        p++;
    }
    if (ti < 0) {
        LOG_WARN("Gemini: no \"text\" field found. HTTP %d, body len %u", code, (unsigned)resp.length());
        server.send(503, "application/json", "{\"error\":\"parse_failed\"}"); return;
    }
    String inner;
    inner.reserve(128);
    {
        bool esc = false, started = false, inStr = false;
        int depth = 0;
        for (int i = ti; i < (int)resp.length(); i++) {
            char c = resp[i];
            if (esc) {
                if (started) {
                    if      (c == '"')  inner += '"';
                    else if (c == '\\') inner += '\\';
                    else if (c == 'n')  inner += '\n';
                    else if (c == 't')  inner += '\t';
                    else if (c == 'r')  inner += '\r';
                    // \uXXXX and other escapes: append literal char (harmless for
                    // numeric fields; \u in name is uncommon given ASCII-only prompt)
                    else                inner += c;
                }
                esc = false; continue;
            }
            if (c == '\\') { esc = true; continue; }
            if (!started) { if (c == '{') { started = true; depth = 1; inner += c; } continue; }
            inner += c;
            if (c == '"') { inStr = !inStr; continue; }
            if (inStr) continue;   // braces inside string values don't affect depth
            if      (c == '{') depth++;
            else if (c == '}') { if (--depth == 0) break; }
        }
    }
    if (inner.length() < 10) {
        LOG_WARN("Gemini: inner JSON too short (%u chars). Partial resp: %.200s",
                 (unsigned)inner.length(), resp.c_str());
        server.send(503, "application/json", "{\"error\":\"parse_failed\"}"); return;
    }

    // Both helpers skip optional whitespace after ':' so they work with
    // compact ("key":"val") and pretty-printed ("key": "val") inner JSON.
    auto exStr = [&](const char *fld) -> String {
        String s = String("\"") + fld + "\":";
        int i = inner.indexOf(s); if (i < 0) return "";
        i += s.length();
        while (i < (int)inner.length() && inner[i] == ' ') i++;
        if (i >= (int)inner.length() || inner[i] != '"') return "";
        i++;
        // Scan for closing '"', skipping '\\' escape pairs so a name that
        // contained '\"' in the original JSON (unescaped to '"' by the
        // extractor loop above) doesn't truncate the value prematurely.
        int e = i;
        while (e < (int)inner.length()) {
            if (inner[e] == '\\') { e += 2; continue; }
            if (inner[e] == '"')  break;
            e++;
        }
        return (e > i) ? inner.substring(i, e) : "";
    };
    auto exNum = [&](const char *fld) -> int {
        String s = String("\"") + fld + "\":";
        int i = inner.indexOf(s); if (i < 0) return -1;
        i += s.length();
        while (i < (int)inner.length() && inner[i] == ' ') i++;
        int e = i;
        while (e < (int)inner.length() && (isdigit((unsigned char)inner[e]) || (e == i && inner[e] == '-'))) e++;
        return (e > i) ? inner.substring(i, e).toInt() : -1;
    };

    String name = exStr("name");
    int b  = exNum("b"),  cv = exNum("c"),  co = exNum("co");
    int sp = exNum("sp"), bl = exNum("bl"), th = exNum("th");
    if (name.length() == 0 || b < 0) {
        LOG_WARN("Gemini: missing fields in inner JSON: name=\"%s\" b=%d. inner: %.120s",
                 name.c_str(), b, inner.c_str());
        server.send(503, "application/json", "{\"error\":\"parse_failed\"}"); return;
    }

    if (b  >= 0) setBright(constrain(b,  0, 100));
    if (cv >= 0) { uiContrast = (uint8_t)constrain(cv, 0,   100); }
    if (co >= 0) { uiCooling  = (uint8_t)constrain(co, 20,  150); recalcCooling(); }
    if (sp >= 0) { uiSparking = (uint8_t)constrain(sp, 0,   255); }
    if (bl >= 0) { uiBlend    = (uint8_t)constrain(bl, 0,   255); }
    if (th >= 0) { uiTheme    = (uint8_t)constrain(th, 0,   THEME_COUNT - 1); }
    buildHeatPalette();
    updatePowerCalc();
    markDirty();

    // Truncate by Unicode codepoints (same logic as handleSavePreset) to avoid
    // splitting a multibyte UTF-8 sequence at a byte boundary.
    if (name.length() > (unsigned)PRESET_NAME_MAX_LEN) {
        int bytePos = 0, chars = 0;
        while (bytePos < (int)name.length() && chars < PRESET_NAME_MAX_LEN) {
            uint8_t ch = (uint8_t)name[bytePos];
            int seqLen = (ch < 0x80) ? 1 : (ch < 0xC0) ? 1 : (ch < 0xE0) ? 2 : (ch < 0xF0) ? 3 : 4;
            if (bytePos + seqLen > (int)name.length()) break;
            bytePos += seqLen;
            chars++;
        }
        name = name.substring(0, bytePos);
    }
    char j[192];
    snprintf(j, sizeof(j),
             "{\"ok\":true,\"name\":\"%s\",\"b\":%d,\"c\":%d,\"co\":%d,"
             "\"sp\":%d,\"bl\":%d,\"th\":%d,\"w\":%.1f}",
             jsonEscape(name).c_str(),
             (int)uiBright, (int)uiContrast, (int)uiCooling,
             (int)uiSparking, (int)uiBlend, (int)uiTheme,
             (float)currentPowerMw / 1000.0f);
    server.send(200, "application/json", j);
    wsPushState();
}

void registerGeminiHandlers() {
    server.on("/setgeminikey", HTTP_POST, handleSetGeminiKey);
    server.on("/geminikey",    handleGeminiKey);
    server.on("/surprise",     handleSurprise);
}

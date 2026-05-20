# FireLamp — Copilot Instructions

ESP32-S3 firmware for an 800-LED WS2812B fire-effect lamp (20 col × 40 rows).
Web UI served from PROGMEM over WiFi. OTA updates from GitHub Releases.
Always commit and push directly to `main`. Do not create feature branches unless asked.

## File map — read only what you need

| Task | File |
|------|------|
| New HTTP endpoint | The module that owns it (see Module ownership) |
| New constant / limit | `src/config.h` |
| New shared variable | `src/globals.h` + `src/main.cpp` |
| Fire physics / palette | `src/fire.cpp` |
| Web UI (HTML/CSS/JS) | `src/page.h` |
| WiFi startup / mDNS | `src/network.cpp` |
| OTA / version check | `src/ota.cpp` |
| AI Surprise Me effect | `src/gemini.cpp` |
| Preset CRUD | `src/presets.cpp` |
| Shared HTTP utilities | `src/net_helpers.h` |

Never read `page.h` for C++ changes. Never read all files for a single-concern edit.

## Module ownership

```
handlers.cpp  →  registerBasicHandlers()   setb/c/co/sp/bl/theme, reset, info, debug, resetwifi, root
presets.cpp   →  registerPresetHandlers()  getpresets, savepreset, loadpreset, deletepreset
ota.cpp       →  registerOtaHandlers()     checkupdate, update; + startAutoUpdateTask()
gemini.cpp    →  registerGeminiHandlers()  surprise, setgeminikey, geminikey
network.cpp   →  startNetwork() / serviceNetwork() — no handlers, only wiring
```

To add a new endpoint: handler + `server.on()` inside the relevant `registerXxx()`. Do NOT touch `network.cpp`.

## Shared utilities (src/net_helpers.h)

```cpp
bool   isWebRequest();                              // CSRF guard — top of every mutating handler
bool   parseIntArg(const char*, int lo, int hi, int&);
String jsonEscape(const String&);
void   sendVal();                                   // send {b,c,co,sp,w,bl,th,upd}
bool   flushPrefs();
void   markDirty();                                 // globals.h inline — Core 1 only
```

## Concurrency rules

- All variables shared Core 0 ↔ Core 1 use `std::atomic<T>`. Never use plain types.
- `markDirty()`, `buildHeatPalette()`, `recalcCooling()` — Core 1 only.
- `windDir[]`, `windTarget[]`, `lastWindChange` — Core 0 only.

## Handler pattern

```cpp
static void handleXxx() {
    if (!isWebRequest()) return;
    int v;
    if (!parseIntArg("v", LO, HI, v)) {
        server.send(400, "application/json", "{\"error\":\"invalid\"}"); return;
    }
    markDirty();
    sendVal();
}
```

## Code style

- Comments only for non-obvious WHY. No comments explaining WHAT.
- No error handling for impossible cases.
- Magic numbers → `config.h` constants.
- `sizeof(buf)` in snprintf, never hardcode sizes.

Full details: see `AGENTS.md` and `CODING.md`.

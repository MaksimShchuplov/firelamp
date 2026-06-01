# FireLamp — AI Agent Instructions

ESP32-S3 firmware for an 800-LED WS2812B fire-effect lamp (20 col × 40 rows).
Web UI served from PROGMEM over WiFi. OTA updates from GitHub Releases.
Always commit and push directly to `main`. Do not create feature branches unless asked.

## Build

```bash
pio run -e esp32s3                      # compile
pio run -e esp32s3 -t upload            # flash via USB
pio device monitor --baud 115200        # serial output
```

## File map — read only what you need

| Task | File |
|------|------|
| New HTTP endpoint | The module that owns it (see Module ownership below) |
| New constant / limit | `src/config.h` |
| New shared variable | `src/globals.h` + `src/main.cpp` |
| Fire physics / palette | `src/fire.cpp` |
| Web UI (HTML/CSS/JS) | `ui/index.html`, `ui/css/*.css`, `ui/js/*.js` — edit sources, not generated `src/page.h` |
| WiFi startup / mDNS | `src/network.cpp` |
| OTA / version check | `src/ota.cpp` |
| AI Surprise Me effect | `src/gemini.cpp` |
| Preset CRUD | `src/presets.cpp` |
| Shared HTTP utilities | `src/net_helpers.h` |

Never read `page.h` for C++ changes. Never read all files for a single-concern edit.

## Module ownership

Each module owns its routes; `network.cpp` only wires them together.

```
handlers.cpp  →  registerBasicHandlers()   setb/c/co/sp/bl/theme, reset, info, debug, resetwifi, root
presets.cpp   →  registerPresetHandlers()  getpresets, savepreset, loadpreset, deletepreset
ota.cpp       →  registerOtaHandlers()     checkupdate, update; + startAutoUpdateTask()
gemini.cpp    →  registerGeminiHandlers()  surprise, setgeminikey, geminikey
network.cpp   →  startNetwork() / serviceNetwork() — no handlers, only wiring
```

To add a new endpoint: add handler + `server.on()` inside the relevant `registerXxx()`. Do NOT touch `network.cpp`.

## Shared utilities — never re-implement

Declared in `src/net_helpers.h`, implemented in `src/handlers.cpp`:

```cpp
bool   isWebRequest();                              // CSRF guard — add at top of every mutating handler
bool   parseIntArg(const char*, int lo, int hi, int&); // parse + range-check query param
String jsonEscape(const String&);                   // escape for JSON string value
void   sendVal();                                   // send current lamp state {b,c,co,sp,w,bl,th,upd}
bool   flushPrefs();                                // write 6 UI params to NVS
void   markDirty();                                 // (globals.h inline) mark NVS dirty, reset debounce
```

## Concurrency — never break these

- **All** variables shared between Core 0 (LEDTask) and Core 1 (network) use `std::atomic<T>`.
- `markDirty()`, `buildHeatPalette()`, `recalcCooling()` — Core 1 only.
- `windDir[]`, `windTarget[]`, `lastWindChange` — Core 0 only.
- `heatPalette` uses double-buffer + `seq_cst` atomic index flip (`activePal`).

## HTTP handler pattern

```cpp
static void handleXxx() {
    if (!isWebRequest()) return;           // CSRF — all mutating endpoints
    int v;
    if (!parseIntArg("v", LO, HI, v)) {
        server.send(400, "application/json", "{\"error\":\"invalid\"}"); return;
    }
    // apply change
    markDirty();
    sendVal();
}
```

CSRF exceptions (read-only, no outbound requests): `/state`, `/info`, `/log`, `/getpresets`, `/geminikey`.
`/debug` keeps the guard — it exposes SSID, heap stats, and all tuning params.

## Code style

- No comments explaining WHAT — names do that. Comments only for non-obvious WHY.
- No error handling for impossible cases (internal calls, framework guarantees).
- Magic numbers → named constants in `config.h`.
- Buffer sizes: always use `sizeof(buf)` in `snprintf`, never hardcode.

## Key constants (src/config.h)

`COLUMNS 20` · `ROWS 40` · `NUM_LEDS 800` · `PRESET_COUNT 8` · `THEME_COUNT 4`
`BRIGHT_DEFAULT 100` · `CONTRAST_DEFAULT 50` · `COOLING_DEFAULT 46` · `SPARKING_DEFAULT 26`
`BLEND_DEFAULT 50` · `THEME_DEFAULT 0` (0=Fire 1=Ember 2=Plasma 3=Ice)
`NVS_COMMIT_DELAY_MS 2500` · `GEMINI_TIMEOUT_MS 25000` · `UPDCHK_STACK_BYTES 12288`

## HTTP API

All state-mutating endpoints require `X-Requested-With: firelamp` header (CSRF).

| Endpoint | Params | Notes |
|----------|--------|-------|
| `GET /state` | — | `{b,c,co,sp,w,bl,th,upd}` |
| `GET /setb` | `v=0..100` | brightness |
| `GET /setc` | `v=0..100` | contrast |
| `GET /setco` | `v=20..150` | cooling |
| `GET /setsp` | `v=0..255` | sparking |
| `GET /setbl` | `v=0..255` | blend |
| `GET /settheme` | `v=0..3` | theme |
| `GET /reset` | — | restore defaults |
| `GET /checkupdate` | — | compare version (60 s cache) |
| `GET /update` | — | start OTA; reboots on success |
| `GET /info` | — | flash_mb, free_heap, ip, version, build |
| `GET /getpresets` | — | array of 8 preset slots |
| `GET /savepreset` | `slot=0..7&name=str` | save current state |
| `GET /loadpreset` | `slot=0..7` | load preset |
| `GET /deletepreset` | `slot=0..7` | clear slot |
| `GET /resetwifi` | — | clear credentials + reboot |
| `POST /setgeminikey` | body: `key=<str>` | save Gemini key to NVS |
| `GET /geminikey` | — | `{"set":true/false}` |
| `GET /surprise` | — | Synchronous Gemini call (blocks ≤25 s, ~2 s with thinking off); HTTP 200 with full state + `"name"` |

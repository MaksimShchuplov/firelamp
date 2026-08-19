# Coding Guidelines — Token-Efficient Development

These rules exist so AI-assisted edits stay scoped: touch one file, load one file.

## File routing — read only what you need

| Task | Read |
|------|------|
| New HTTP endpoint | The module that owns it (handlers/presets/ota/gemini/mqtt/flash/pwa) |
| New constant / limit | `config.h` only |
| Slider param range | `config.h` only (`*_MIN`/`*_MAX`) — single source for HTTP, MQTT, NVS clamp |
| Param registry (key/range/default/side-effect) | `params.cpp` (+ `params.h` for `ParamDesc` / `applyJsonParams`) |
| New shared variable | `globals.h` only |
| Fire physics / palette | `fire.cpp` only |
| Web UI layout / HTML | `ui/index.html` only |
| Web UI styles / CSS  | `ui/css/<name>.css` only |
| Web UI logic / JS    | `ui/js/<name>.js` only |
| Startup / WiFi / mDNS | `network.cpp` only |
| OTA / version check | `ota.cpp` only |
| AI Surprise Me | `gemini.cpp` only |
| Preset CRUD | `presets.cpp` only |
| MQTT broker / config | `mqtt.cpp` + `ui/js/mqtt.js` |
| Manual flash recovery (`/flash`) | `flash.cpp` only |
| PWA manifest / service worker | `pwa.cpp` only |
| Browser poll / offline detection | `ui/js/poll.js` + `ui/js/state.js` |
| Cross-module signature | `net_helpers.h` + the one relevant `.cpp` |

Never read all files. Never read UI files for C++ changes. `src/page.h` is generated — edit `ui/` sources instead.

## Module ownership — each module registers its own routes

```
handlers.cpp  →  registerBasicHandlers()   — root, state, setb/c/co/sp/bl/theme, reset, info, debug, log, resetwifi
presets.cpp   →  registerPresetHandlers()  — getpresets, savepreset, loadpreset, deletepreset
ota.cpp       →  registerOtaHandlers()     — checkupdate, update; plus startAutoUpdateTask()
gemini.cpp    →  registerGeminiHandlers()  — surprise, setgeminikey, geminikey
mqtt.cpp      →  registerMqttHandlers()    — setmqtt, getmqtt; plus initMqtt(), serviceMqtt()
flash.cpp     →  registerFlashHandlers()   — /flash (GET form + POST upload)
pwa.cpp       →  registerPwaHandlers()     — manifest.json, sw.js
network.cpp   →  startNetwork() / serviceNetwork() — no handlers, just wiring
params.cpp    →  PARAMS[] registry         — one row per UI param: key, atomic, default, range, side-effect; drives presets + MQTT/Gemini JSON apply
```

To add a new endpoint: add handler + `server.on()` inside the relevant `registerXxx()`.
Do NOT touch `network.cpp` or `net_helpers.h` for a single endpoint addition.

## Shared utilities — already available, never re-implement

Declared in `net_helpers.h`, implemented in `handlers.cpp`:

```cpp
bool   isWebRequest();                             // CSRF guard — call at top of mutating handlers
bool   parseIntArg(const char*, int lo, int hi, int&); // parse + range-check a query param
String jsonEscape(const String&);                  // escape for JSON string value
void   sendVal();                                  // send current lamp state as JSON
bool   flushPrefs();                               // write 6 UI params to NVS
inline void markDirty();                           // in globals.h — mark NVS dirty, reset debounce
```

Before changing anything that alters how the lamp *looks*, read `ROADMAP.md` —
its "Rejected" section records visual behaviour that looks like a bug, is not,
and has already been "fixed" and reverted more than once.

## Concurrency rules — never break these

- All variables shared Core 0 ↔ Core 1 are `std::atomic<T>`. Do not use plain types.
- `markDirty()` — Core 1 only (network handlers). Never call from Core 0.
- `windDir[]`, `windTarget[]`, `lastWindChange` — Core 0 only. Never read/write from Core 1.
- `buildHeatPalette()` — Core 1 only. Double-buffer + seq_cst flip keeps Core 0 safe.
- `recalcCooling()` — Core 1 only. Writes `coolMax[]` (atomic array, read Core 0).

## HTTP handler pattern

```cpp
static void handleXxx() {
    if (!isWebRequest()) return;          // CSRF guard (all mutating endpoints)
    int v;
    if (!parseIntArg("v", LO, HI, v)) {
        server.send(400, "application/json", "{\"error\":\"invalid\"}"); return;
    }
    // apply change
    markDirty();
    sendVal();                            // returns current state including w (watts)
}
```

CSRF guard exceptions (read-only, no outbound requests): `/state`, `/info`, `/log`, `/getpresets`, `/geminikey`, `/getmqtt`, `/sw.js`, `/manifest.json`.
`/debug` is NOT an exception — it exposes SSID, heap details, and all tuning params, so it keeps the guard.

## Code style

- No comments explaining WHAT the code does — names do that.
- Comments only for non-obvious WHY: hardware constraints, cross-core invariants, subtle limits.
- No error handling for impossible cases (internal calls, framework guarantees).
- No backwards-compat shims unless explicitly asked.
- Buffer sizes: use `sizeof(buf)` in snprintf, never hardcode. Validate at system boundaries only.
- Magic numbers → named constants in `config.h`. Never inline numbers in logic.

## Adding a new UI parameter

Checklist (in order):

1. `config.h` — add `FOO_DEFAULT`, range limits as `#define`
2. `globals.h` — add `extern std::atomic<uint8_t> uiFoo;`
3. `main.cpp` — add definition `std::atomic<uint8_t> uiFoo{FOO_DEFAULT};`
4. `params.cpp` — add `PARAMS[]` row `{"fo", &uiFoo, FOO_DEFAULT, FOO_MIN, FOO_MAX, sideEffectOrNullptr, true}` — this alone makes presets, MQTT `firelamp/set`, and Gemini pick it up
5. `handlers.cpp` — add `handleSetFoo()` + wire in `registerBasicHandlers()`
6. `handlers.cpp:flushPrefs()` — add `prefs.putUChar("foo", uiFoo)`
7. `network.cpp:loadSettings()` — load + clamp from NVS
8. `handlers.cpp:formatState()` — add field to JSON response
9. `fire.cpp` — use `uiFoo` in simulation
10. `ui/js/globals.js` — add DOM ref and debounce timer variable
11. `ui/js/state.js` — add `pfo(n)` apply function
12. `ui/js/lang.js` — add label text + `ul()` translations (EN + RU); `ui/js/globals.js` — `DD` description table entry
13. `ui/js/sliders.js` — add slider event listener
14. `ui/index.html` — add slider, label, description `<div>`
15. `ui/css/sliders.css` — add styles if needed

Each step is one file. Read only that file.

## UI conventions (ui/js/)

- All UI text goes through `ul()` in `lang.js` for EN/RU switching. Add both languages.
- `xf(url)` — fetch with CSRF header. Use for all mutating calls (sliders, presets, OTA, etc.).
- `pb/pc/pco/psp/pbl/pth(n)` — apply server value to UI (`state.js`). Add `pfo(n)` for new params.
- Slider debounce: 120 ms timeout; declare timer variable in `globals.js`, handler in `sliders.js`.
- `dynDesc('sid', val)` — maps value range to description text from `DD` table in `globals.js`.
- `build_page.py` assembles `ui/` into `src/page.h` at pre-build time. Order is declared in `CSS_FILES` / `JS_FILES` lists in `build_page.py`.

## Tests — run before every commit

```bash
make test                            # all suites: C++ (Unity), UI JS (node:test), Python (pytest)
node --check ui/js/<changed>.js      # syntax check any edited JS (it ships minified in PROGMEM)
```

No hardware needed — everything is native and takes <5 s total. If a change
touches a header-only helper (`text_utils.h`, `ota_utils.h`, `mqtt_state.h`) or a
formula mirrored in `test/native/test_main.cpp`, update the test in the same commit.

`test/test_consistency.py` cross-checks config.h ranges/defaults against their UI
mirrors (index.html slider attrs, state.js clamps, sliders.js vib bounds, globals.js
DD buckets, presets.js import keys vs params.cpp). If it fails after your edit, you
changed one side of a mirrored value — fix the other side, don't weaken the test.

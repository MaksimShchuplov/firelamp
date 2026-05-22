# Coding Guidelines — Token-Efficient Development

These rules exist so AI-assisted edits stay scoped: touch one file, load one file.

## File routing — read only what you need

| Task | Read |
|------|------|
| New HTTP endpoint | The module that owns it (handlers/presets/ota/gemini) |
| New constant / limit | `config.h` only |
| New shared variable | `globals.h` only |
| Fire physics / palette | `fire.cpp` only |
| Web UI (HTML/CSS/JS) | `page.h` only |
| Startup / WiFi / mDNS | `network.cpp` only |
| OTA / version check | `ota.cpp` only |
| AI Surprise Me | `gemini.cpp` only |
| Preset CRUD | `presets.cpp` only |
| Cross-module signature | `net_helpers.h` + the one relevant `.cpp` |

Never read all files. Never read `page.h` for C++ changes.

## Module ownership — each module registers its own routes

```
handlers.cpp  →  registerBasicHandlers()   — setb/c/co/sp/bl/theme, reset, info, debug, resetwifi, root
presets.cpp   →  registerPresetHandlers()  — getpresets, savepreset, loadpreset, deletepreset
ota.cpp       →  registerOtaHandlers()     — checkupdate, update; plus startAutoUpdateTask()
gemini.cpp    →  registerGeminiHandlers()  — surprise, setgeminikey, geminikey
network.cpp   →  startNetwork() / serviceNetwork() — no handlers, just wiring
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

CSRF guard exceptions (read-only, no outbound requests): `/state`, `/info`, `/getpresets`, `/geminikey`.

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
4. `handlers.cpp` — add `handleSetFoo()` + wire in `registerBasicHandlers()`
5. `handlers.cpp:flushPrefs()` — add `prefs.putUChar("foo", uiFoo)`
6. `network.cpp:startNetwork()` — load + clamp from NVS
7. `handlers.cpp:sendVal()` — add field to JSON response
8. `fire.cpp` — use `uiFoo` in simulation
9. `page.h` — add slider, label, `pfo()` init function, `DD` description table, `ul()` translations

Each step is one file. Read only that file.

## page.h conventions

- All text goes through `ul()` for EN/RU switching. Add both languages.
- `xf(url)` — fetch with CSRF header. Use for all mutating calls.
- `xfc(url)` — same, with offline detection. Use for slider debounce calls.
- `pb/pc/pco/psp/pbl/pth(n)` — apply server value to UI. Add `pfo(n)` for new params.
- Slider debounce: 120 ms timeout, use a new `tN` variable.
- `dynDesc('sid', val)` — maps value range to description text from `DD` table.

#pragma once
#include <Arduino.h>

// =============================================================================
//  Shared HTTP utilities — implemented in handlers.cpp
// =============================================================================

// CSRF guard: rejects requests without X-Requested-With: firelamp header.
// Browser CORS pre-flight fails for cross-origin requests that set custom headers,
// so any request that reaches this check with the header set came from our own page.
[[nodiscard]] bool isWebRequest();

// Parses a bounded integer query arg. Returns false on failure; caller sends the error response.
[[nodiscard]] bool parseIntArg(const char *name, int lo, int hi, int &out);

// JSON string escaping.
[[nodiscard]] String jsonEscape(const String &s);

// Formats current lamp state into buf as JSON {b,c,co,sp,w,bl,th,upd}.
void formatState(char *buf, size_t len);

// Sends current lamp state as JSON {b,c,co,sp,w,bl,th,upd}.
void sendVal();

// Writes all 6 UI params to NVS via the global prefs handle. Returns true on success.
[[nodiscard]] bool flushPrefs();

// =============================================================================
//  Route registration — each module calls server.on() for its own handlers
// =============================================================================

void registerBasicHandlers();   // handlers.cpp: root, set*, reset, info, debug, resetwifi
void registerPresetHandlers();  // presets.cpp:  getpresets, savepreset, loadpreset, deletepreset
void registerOtaHandlers();     // ota.cpp:      checkupdate, update
void startAutoUpdateTask();     // ota.cpp:      launch background version-check FreeRTOS task
void registerGeminiHandlers();  // gemini.cpp:   surprise, setgeminikey, geminikey

#pragma once
#include <atomic>
#include <FastLED.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include "config.h"

// ---- LED buffers -----------------------------------------------------------
extern CRGB    leds[NUM_LEDS];
extern uint8_t heat[ROWS][COLUMNS];

// Double-buffered palette. buildHeatPalette() writes the inactive buffer then
// flips activePal with a seq_cst atomic store (acts as full memory barrier —
// ensures all palette stores are visible to Core 0 before the index flip).
// fireEffect() snapshots activePal once per frame so a mid-frame flip cannot
// cause split-palette rendering.
extern CRGB                 heatPalette[2][256];
extern std::atomic<uint8_t> activePal;

// ---- Wind state ------------------------------------------------------------
// windDir, windTarget, lastWindChange are accessed exclusively from LEDTask (Core 0).
// Do NOT read or write them from Core 1 network handlers without a mutex.
extern float             windDir[ROWS];
extern float             windTarget[ROWS];
extern std::atomic<uint8_t> coolMax[ROWS];  // written Core 1 (recalcCooling), read Core 0
extern uint32_t          lastWindChange;  // LEDTask-only

// ---- Shared singletons -----------------------------------------------------
extern WebServer   server;
extern WiFiManager wm;
extern Preferences prefs;

// ---- UI parameters (std::atomic: written Core 1, read Core 0) -------------
// std::atomic<T> makes atomicity a language contract, not an ISA assumption.
// Changing a parameter's underlying type no longer silently breaks the
// cross-core contract the way volatile + "trust the ISA" would.
extern std::atomic<uint8_t>  uiBright;
extern std::atomic<uint8_t>  uiContrast;
extern std::atomic<uint8_t>  uiCooling;
extern std::atomic<uint8_t>  uiSparking;
extern std::atomic<uint8_t>  uiBlend;
extern std::atomic<uint8_t>  uiTheme;
extern std::atomic<uint8_t>  appliedRaw;
extern std::atomic<uint32_t> currentPowerMw;  // milliwatts
extern std::atomic<bool>     updatePending;

// ---- NVS deferred-write state (Core 1 only) ------------------------------------
// prefsDirty/prefsTouch are written by markDirty() — called from HTTP handlers
// AND from surpriseTask (a separate FreeRTOS task on Core 1). Both tasks share
// Core 1 under preemptive scheduling; plain types are formally data races. Both atomic.
extern std::atomic<bool>     prefsDirty;
extern std::atomic<uint32_t> prefsTouch;
// Written from WiFi event callback (Core 0 WiFi task) and read/written from
// serviceNetwork (Core 1). Must be atomic to avoid a formal data race on LX7.
extern std::atomic<uint32_t> wifiRetryAt;

// Marks NVS dirty and resets the debounce timer. Call from Core 1 only.
inline void markDirty() { prefsDirty.store(true, std::memory_order_relaxed); prefsTouch.store(millis(), std::memory_order_relaxed); }
extern std::atomic<uint32_t> lastPowerCalc;  // written Core 0 (fireEffect) + Core 1 (handlers)

// Last applied Gemini effect — used by /state HTTP fallback for browsers that cannot reach WS port 81.
// Written by surpriseTask (Core 1), read by handleState (Core 1 serviceNetwork). Both same core,
// preemptive FreeRTOS; lastSurpriseAt release/acquire orders the string write before the timestamp.
extern char                  lastSurpriseName[16];   // jsonEscaped, null-terminated
extern std::atomic<uint32_t> lastSurpriseAt;         // millis() when set; 0 = never

// ---- Task handles ----------------------------------------------------------
extern TaskHandle_t ledTaskHandle;   // set in setup(); used for stack watermark queries

// ---- Structured logging ----------------------------------------------------
// All log output includes a millisecond timestamp so serial captures are
// trivially grep-able for [ERROR] / [WARN] even without an RTC.
#define LOG_ERROR(fmt, ...) Serial.printf("[ERROR] %7lums " fmt "\n", millis(), ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  Serial.printf("[WARN]  %7lums " fmt "\n", millis(), ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  Serial.printf("[INFO]  %7lums " fmt "\n", millis(), ##__VA_ARGS__)

// ---- Function declarations (implemented across modules) --------------------
void buildHeatPalette();
void applyBrightness();
void setBright(int v);
void recalcCooling();
void updatePowerCalc();
void fireEffect();
void updateWind();

void safeBootCheck();
void markBootSuccess();

void startNetwork();
void serviceNetwork();

void wsSetup();
void wsLoop();
void wsPushState();
// Broadcasts state JSON with an added "name" field — used by the async Gemini task
// to deliver the effect name to all connected browsers via WebSocket.
void wsPushSurprise(const char *escapedName);

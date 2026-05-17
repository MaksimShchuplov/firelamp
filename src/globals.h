#pragma once
#include <FastLED.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include "config.h"

// ---- LED buffers -----------------------------------------------------------
extern CRGB    leds[NUM_LEDS];
extern uint8_t heat[ROWS][COLUMNS];

// Double-buffered palette. buildHeatPalette() writes the inactive buffer then
// flips activePal (single-byte atomic write). fireEffect() snapshots activePal
// once per frame so a mid-frame flip cannot cause split-palette rendering.
extern CRGB             heatPalette[2][256];
extern volatile uint8_t activePal;

// ---- Wind state ------------------------------------------------------------
extern float    windDir[ROWS];
extern float    windTarget[ROWS];
extern uint8_t  coolMax[ROWS];
extern uint32_t lastWindChange;

// ---- Shared singletons -----------------------------------------------------
extern WebServer   server;
extern WiFiManager wm;
extern Preferences prefs;

// ---- UI parameters (volatile: written Core 1, read Core 0) ----------------
// uint8_t single-byte writes are atomic on LX7; volatile prevents the compiler
// from register-caching values across the FreeRTOS task-switch boundary.
extern volatile uint8_t uiBright;
extern volatile uint8_t uiContrast;
extern volatile uint8_t uiCooling;
extern volatile uint8_t uiSparking;
extern volatile uint8_t appliedRaw;
extern volatile float   currentPowerW;

// ---- NVS deferred-write state (Core 1 only) --------------------------------
extern bool     prefsDirty;
extern uint32_t prefsTouch;
extern uint32_t wifiRetryAt;
extern uint32_t lastPowerCalc;

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

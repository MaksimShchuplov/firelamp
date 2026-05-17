#include <Arduino.h>
#include <FastLED.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include "globals.h"

// =============================================================================
//  GLOBAL DEFINITIONS  (extern declarations live in globals.h)
// =============================================================================

CRGB     leds[NUM_LEDS];
uint8_t  heat[ROWS][COLUMNS];

CRGB             heatPalette[2][256];
volatile uint8_t activePal = 0;

float    windDir[ROWS];
float    windTarget[ROWS];
uint8_t  coolMax[ROWS];
uint32_t lastWindChange = 0;

WebServer   server(80);
WiFiManager wm;
Preferences prefs;

volatile uint8_t uiBright     = BRIGHT_DEFAULT;
volatile uint8_t uiContrast   = 50;
volatile uint8_t uiCooling    = 45;
volatile uint8_t uiSparking   = 36;
volatile uint8_t appliedRaw   = 0;
volatile float   currentPowerW = 0.0f;

bool     prefsDirty  = false;
uint32_t prefsTouch  = 0;
uint32_t wifiRetryAt = 0;
uint32_t lastPowerCalc = 0;

// =============================================================================
//  SETUP / LOOP
// =============================================================================

void setup() {
    Serial.begin(115200);
    safeBootCheck();

    FastLED.addLeds<WS2812B, LED_PIN, LED_COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setMaxPowerInVoltsAndMilliamps(PSU_VOLTS, 20000);
    FastLED.clear(true);

#if COLOR_TEST
    // Strip MUST show solid RED. Green/blue → wrong LED_COLOR_ORDER in config.h.
    fill_solid(leds, NUM_LEDS, CRGB(255, 0, 0));
    FastLED.show(); delay(1000); FastLED.clear(true);
#endif

    for (int y = 0; y < ROWS; y++) windDir[y] = windTarget[y] = 0.0f;

    startNetwork();   // loads NVS → builds palette → joins WiFi → starts web server

    xTaskCreatePinnedToCore(
        [](void *) {
            for (;;) {
                updateWind();
                fireEffect();
                FastLED.show();
                vTaskDelay(pdMS_TO_TICKS(1));   // yield → feed Core 0 watchdog
            }
        },
        "LEDTask", 4096, NULL, 1, NULL, 0       // pinned to Core 0
    );
}

void loop() {
    serviceNetwork();
    vTaskDelay(pdMS_TO_TICKS(10));              // yield Core 1
}

#include <Arduino.h>
#include <FastLED.h>
#include <WebServer.h>
#include <WiFiManager.h>
#include <Preferences.h>
#include <esp_task_wdt.h>
#include "globals.h"

// =============================================================================
//  GLOBAL DEFINITIONS  (extern declarations live in globals.h)
// =============================================================================

CRGB     leds[NUM_LEDS];
uint8_t  heat[ROWS][COLUMNS];

CRGB             heatPalette[2][256];
std::atomic<uint8_t> activePal{0};

float             windDir[ROWS];
float             windTarget[ROWS];
std::atomic<uint8_t> coolMax[ROWS];
uint32_t          lastWindChange = 0;

WebServer   server(80);
WiFiManager wm;
Preferences prefs;

std::atomic<uint8_t>  uiBright    {BRIGHT_DEFAULT};
std::atomic<uint8_t>  uiContrast  {CONTRAST_DEFAULT};
std::atomic<uint8_t>  uiCooling   {COOLING_DEFAULT};
std::atomic<uint8_t>  uiSparking  {SPARKING_DEFAULT};
std::atomic<uint8_t>  uiBlend     {BLEND_DEFAULT};
std::atomic<uint8_t>  uiTheme     {THEME_DEFAULT};
std::atomic<uint8_t>  appliedRaw  {0};
std::atomic<uint32_t> currentPowerMw{0};
std::atomic<bool>     updatePending{false};

TaskHandle_t ledTaskHandle = NULL;

bool     prefsDirty  = false;
uint32_t prefsTouch  = 0;
uint32_t wifiRetryAt = 0;
std::atomic<uint32_t> lastPowerCalc{0};

// =============================================================================
//  SETUP / LOOP
// =============================================================================

void setup() {
    Serial.begin(115200);
    safeBootCheck();

    FastLED.addLeds<WS2812B, LED_PIN, LED_COLOR_ORDER>(leds, NUM_LEDS);
    FastLED.setMaxPowerInVoltsAndMilliamps(PSU_VOLTS, PSU_MAX_MA);
    FastLED.clear(true);

#if COLOR_TEST
    // Strip MUST show solid RED. Green/blue → wrong LED_COLOR_ORDER in config.h.
    fill_solid(leds, NUM_LEDS, CRGB(255, 0, 0));
    FastLED.show(); delay(1000); FastLED.clear(true);
#endif

    memset(windDir,    0, sizeof(windDir));
    memset(windTarget, 0, sizeof(windTarget));

    startNetwork();   // loads NVS → builds palette → joins WiFi → starts web server

    BaseType_t ok = xTaskCreatePinnedToCore(
        [](void *) {
            // Register with task watchdog so the WDT catches a hung LED loop.
            esp_task_wdt_add(NULL);
            for (;;) {
                updateWind();
                fireEffect();
                FastLED.show();
                esp_task_wdt_reset();
                vTaskDelay(pdMS_TO_TICKS(1));
            }
        },
        "LEDTask", LEDTASK_STACK_BYTES, NULL, 1, &ledTaskHandle, 0   // pinned to Core 0
    );
    if (ok != pdPASS) {
        LOG_ERROR("LEDTask creation failed — rebooting");
        ESP.restart();
    }
    markBootSuccess();  // firmware initialised without crashing — cancel OTA rollback
}

void loop() {
    serviceNetwork();
    vTaskDelay(pdMS_TO_TICKS(10));   // yield Core 1
}

#pragma once

// ---- Strip geometry --------------------------------------------------------
#define LED_PIN             5
#define COLUMNS             20
#define ROWS                40
#define NUM_LEDS            (COLUMNS * ROWS)
#define LED_COLOR_ORDER     GRB     // change to RGB/BRG if base shows wrong color
#define COLOR_TEST          0       // 1 = solid-red boot test for 1 s

// ---- Fire algorithm --------------------------------------------------------
#define SPARK_INTENSITY         240
#define WIND_CHANGE_INTERVAL    2000
#define WIND_MAX_STRENGTH       3
#define WIND_PULSE_PROBABILITY  50
// Temporal blend 0..255: lower = smoother glow-up. At 50 a fresh spark needs
// ~10 frames (~250 ms at 40 FPS) to reach 90% of target — reads as ember catching.
#define BLEND_DEFAULT           50
#define THEME_DEFAULT           0       // 0=Fire 1=Ember 2=Plasma 3=Ice
#define PRESET_COUNT            8

// ---- Power -----------------------------------------------------------------
#define PSU_VOLTS           5
#define PSU_MAX_MA          20000   // above PSU rating: voltage drop limits real draw

// ---- Brightness ------------------------------------------------------------
#define BRIGHT_DEFAULT      100     // 0..100, until UI / NVS overrides
#define CONTRAST_DEFAULT    50
#define COOLING_DEFAULT     45
#define SPARKING_DEFAULT    36
#define BRIGHT_GAMMA        2.2f    // perceptual curve on the slider
#define BRIGHT_FLOOR        4       // min stable raw PWM on WS2812B
#define BRIGHT_DITHER_ON    16      // enable BINARY_DITHER above this raw value

// ---- Task / system ---------------------------------------------------------
#define LEDTASK_STACK_BYTES     8192    // measured watermark ~3.2 KB; 8 KB gives 2× headroom
#define POWER_CALC_INTERVAL_MS  3000

// ---- Themes / presets ------------------------------------------------------
#define THEME_COUNT             4       // Fire / Ember / Plasma / Ice
#define PRESET_NAME_MAX_LEN     15

// ---- Network ---------------------------------------------------------------
#define WIFI_PORTAL_SSID        "FireLamp-Setup"
#define WIFI_PORTAL_TIMEOUT_S   120     // portal auto-closes; fire still runs
#define WIFI_RETRY_MS           15000
#define HTTP_TIMEOUT_MS         8000    // HTTPS request timeout (version check + OTA)
#define GEMINI_TIMEOUT_MS       15000   // Gemini API response timeout (ESP-side call)
#define MDNS_NAME               "firelamp"   // → http://firelamp.local
#define NVS_COMMIT_DELAY_MS     2500    // defer NVS writes to spare flash endurance
#define FIRMWARE_URL  "https://github.com/MaksimShchuplov/firelamp/releases/latest/download/firmware.bin"
#define VERSION_URL   "https://github.com/MaksimShchuplov/firelamp/releases/latest/download/version.json"
#ifndef FIRMWARE_VERSION
  #define FIRMWARE_VERSION "dev"
#endif
#ifndef BUILD_N
  #define BUILD_N 0
#endif

// ---- Fire algorithm tuning constants (magic numbers extracted) -------------
#define SPARK_MIN_VARIANCE      40      // random range width for spark heat: [SPARK_INTENSITY-SPARK_MIN_VARIANCE .. SPARK_INTENSITY]
#define WIND_LERP_ALPHA         0.1f    // exponential smoothing coefficient for wind direction
#define COOLING_VARIANCE        10      // ±window around uiCooling for per-row randomisation
#define COOLING_ROW_SCALE       10      // maps cooling units to heat-per-row
#define COOLING_ROW_BIAS        2       // minimum cooling applied regardless of uiCooling

// ---- OTA / update check ----------------------------------------------------
#define VERSION_CACHE_MS        60000   // re-fetch version.json at most once per minute
#define OTA_CHECK_DELAY_MS      8000    // delay after WiFi connect before background OTA check
#define UPDCHK_STACK_BYTES      12288   // autoUpdateCheck: TLS handshake + HTTPClient needs ~10 KB stack

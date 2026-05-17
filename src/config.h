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
#define FIRE_BLEND              50

// ---- Power -----------------------------------------------------------------
#define PSU_VOLTS           5
// MAX_PSU_MA hardcoded to 20 000 mA in setup() to account for voltage drop

// ---- Brightness ------------------------------------------------------------
#define BRIGHT_DEFAULT      100     // 0..100, until UI / NVS overrides
#define BRIGHT_GAMMA        2.2f    // perceptual curve on the slider
#define BRIGHT_FLOOR        4       // min stable raw PWM on WS2812B
#define BRIGHT_DITHER_ON    16      // enable BINARY_DITHER above this raw value

// ---- Network ---------------------------------------------------------------
#define WIFI_PORTAL_SSID        "FireLamp-Setup"
#define WIFI_PORTAL_TIMEOUT_S   120     // portal auto-closes; fire still runs
#define WIFI_RETRY_MS           15000
#define MDNS_NAME               "firelamp"   // → http://firelamp.local
#define NVS_COMMIT_DELAY_MS     2500    // defer NVS writes to spare flash endurance
#define FIRMWARE_URL  "https://github.com/MaksimShchuplov/firelamp/releases/latest/download/firmware.bin"
#define VERSION_URL   "https://github.com/MaksimShchuplov/firelamp/releases/latest/download/version.json"
#ifndef FIRMWARE_VERSION
  #define FIRMWARE_VERSION "dev"
#endif

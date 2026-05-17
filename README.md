# FireLamp

[![Build](https://github.com/MaksimShchuplov/firelamp/actions/workflows/build.yml/badge.svg)](https://github.com/MaksimShchuplov/firelamp/actions/workflows/build.yml)

ESP32-S3 firmware for an 800-LED WS2812B fire-effect lamp with a web control UI and OTA updates over Wi-Fi.

![UI screenshot](docs/screenshot.png)

## Features

- Realistic fire simulation — cooling, sparking, wind, temporal blending
- Four color themes: Fire, Ember, Plasma, Ice
- Responsive web UI served directly from the ESP32 (no app, no cloud)
- Four saveable parameter presets
- OTA firmware updates from GitHub Releases — one tap in the UI
- Automatic rollback on three consecutive crashes
- Wi-Fi provisioning via captive portal — credentials never compiled in

## Hardware

| Part | Notes |
|------|-------|
| ESP32-S3 DevKitC-1 | or any ESP32-S3 board |
| WS2812B LED strip | 800 LEDs arranged as 20 × 40 (columns × rows) |
| 5 V power supply | ≥ 10 A recommended (Mean Well LRS-60-5 or similar) |
| 300–500 Ω resistor | in series on the data line |
| 1000 µF / 10 V capacitor | across the strip power rails |

### Wiring

```
ESP32-S3 GPIO 5  ──[300 Ω]──  Strip DATA
ESP32-S3 GND     ───────────  Strip GND  ──  PSU GND
PSU 5 V          ───────────  Strip 5 V
```

> The ESP32-S3 outputs 3.3 V logic. Most WS2812B strips accept this directly when the data wire is short (< 30 cm). Add a level shifter (e.g. 74HCT245) for longer runs or if you see glitches.

## Getting Started

### Prerequisites

- [PlatformIO](https://platformio.org/) (CLI or VS Code extension)
- Git

### Build and flash

```bash
git clone https://github.com/MaksimShchuplov/firelamp.git
cd firelamp
pio run -e esp32s3 -t upload
pio device monitor --baud 115200
```

The firmware version is automatically set to the short Git SHA at build time.

### First boot

1. The lamp starts a Wi-Fi access point called **FireLamp-Setup**
2. Connect to it and open **192.168.4.1**
3. Enter your Wi-Fi credentials
4. The lamp reboots and is available at **http://firelamp.local**

Credentials are stored in flash by the ESP32 Wi-Fi driver and survive all OTA updates.

## Web UI

Open `http://firelamp.local` (or the IP shown on the serial monitor).

| Control | Range | Description |
|---------|-------|-------------|
| Brightness | 0–100 | Overall LED output with gamma-2.2 curve |
| Contrast | 0–100 | Palette shift — low = yellows/white, high = deep reds |
| Cooling | 20–150 | How quickly heat dissipates upward — lower = taller flames |
| Sparking | 0–255 | Ignition rate at the base |
| Blend | 0–255 | Temporal smoothing — lower = sharp flicker, higher = slow glow |
| Theme | Fire / Ember / Plasma / Ice | Color palette |
| Presets | 4 slots | Tap to load · Long-press to save or rename |

All parameters are saved to flash automatically and restored on next boot.

## OTA Updates

The UI checks for updates against the [latest GitHub Release](https://github.com/MaksimShchuplov/firelamp/releases/latest). An update check also runs automatically 8 seconds after Wi-Fi connects.

1. Tap **Check for Update** in the UI
2. If a newer build is available, tap **Install Update**
3. The lamp downloads and flashes the new firmware, then reboots automatically
4. The UI shows a progress bar and reloads when the lamp is back online

## Configuration

All tuneable constants are in [`src/config.h`](src/config.h). Key values:

```c
#define LED_PIN         5       // data pin
#define COLUMNS         20      // strip width
#define ROWS            40      // strip height
#define PSU_VOLTS       5
#define PSU_MAX_MA      20000   // FastLED power budget (mA)
```

To adapt the layout, change `COLUMNS` and `ROWS`. `NUM_LEDS` is derived automatically.

## Architecture

See [ARCHITECTURE.md](ARCHITECTURE.md) for a detailed description of the dual-core task split, shared-state concurrency model, double-buffered palette, OTA flow, and NVS layout.

## License

MIT — see [LICENSE](LICENSE).

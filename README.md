# FireLamp

[![Build](https://github.com/MaksimShchuplov/firelamp/actions/workflows/build.yml/badge.svg)](https://github.com/MaksimShchuplov/firelamp/actions/workflows/build.yml)
[![Release](https://img.shields.io/github/v/release/MaksimShchuplov/firelamp?label=latest)](https://github.com/MaksimShchuplov/firelamp/releases/latest)

800-LED WS2812B fire lamp on an ESP32-S3. Browser UI served from the chip, OTA updates straight from GitHub Releases, Home Assistant over MQTT, and Gemini AI effects. No cloud middleman, no app, no compiled-in credentials.

---

## Why this is more interesting than blinking LEDs

**Dual-core real-time with formal memory safety.** Fire physics run on Core 0 at ~40 FPS. HTTP, MQTT, and NVS writes run on Core 1. All shared state uses `std::atomic<T>` with explicit memory ordering (`seq_cst` on the palette flip, `acquire/release` on NVS dirty flags, `relaxed` where staleness is acceptable). Not "it works on this chip" — formally race-free across compiler versions.

**Firmware CD pipeline.** Every push to `main` compiles the firmware, runs the test suite, generates `version.json` (git SHA + MD5 + build number), and publishes a tagged GitHub Release. The ESP polls the latest release on boot, compares build numbers, and flashes in-place — the same blue/green pattern you'd use in a cloud deployment, implemented in 4 KB of flash.

**Rollback on crash.** A crash counter in a separate NVS namespace increments on every hard reset (panic, watchdog). Three consecutive crashes trigger `Update.rollBack()` before the app starts. The counter resets only after the network stack is up and the LED task is running — meaning a firmware update that boots but fails to initialize rolls back automatically.

**Zero secrets in the binary.** Wi-Fi credentials are stored by the ESP32 Wi-Fi driver (survives every OTA update). API keys and MQTT passwords live in NVS namespaces, set at runtime via the web UI. `strings firmware.bin` returns nothing sensitive.

**The build system tests itself.** `build_page.py` minifies and inlines all CSS and JS into a PROGMEM C++ raw-string literal. The test suite covers the minifier edge cases (backslash sequences that would corrupt regex group references, UTF-8 safe truncation, sentinel injection into the raw-string delimiter). `-Werror` on the firmware, pinned library versions, SHA-pinned GitHub Actions.

---

## Features

- Realistic fire simulation — stochastic per-pixel cooling, upward convection, wind gusts, temporal blending
- Four color themes: Fire, Ember, Plasma, Ice
- Responsive web UI served directly from the ESP32 — no CDN, no internet required
- **PWA** — installable on iOS and Android home screen; works offline (cached UI, offline banner)
- **MQTT** — pub/sub compatible with Home Assistant and any MQTT broker
- **Surprise Me** — Gemini 2.5 Flash generates a unique named effect on demand (~2 s response)
- Eight saveable presets with names
- OTA updates from GitHub Releases — one tap; LED strip shows a download progress bar
- Automatic rollback on three consecutive crashes
- Wi-Fi provisioning via captive portal — no credentials compiled in

---

## Hardware

Full assembly guide — BOM, power injection, matrix layout, troubleshooting — in [HARDWARE.md](HARDWARE.md).

| Part | Notes |
|------|-------|
| ESP32-S3 DevKitC-1 | any ESP32-S3 board works |
| WS2812B LED strip | 800 LEDs, 20 columns × 40 rows |
| 5 V PSU | ≥ 10 A — Mean Well LRS-60-5 or similar |
| 300–500 Ω resistor | series on the data line |
| 1000 µF / 10 V capacitor | across strip power rails |

### Wiring

```
ESP32-S3 GPIO 5  ──[300 Ω]──  Strip DATA
ESP32-S3 GND     ───────────  Strip GND  ──  PSU GND
PSU 5 V          ───────────  Strip 5 V
```

> ESP32-S3 outputs 3.3 V logic. Most WS2812B strips accept this directly on short data runs (< 30 cm). Add a 74HCT245 level shifter for longer runs.

---

## Getting Started

### Prerequisites

- [PlatformIO](https://platformio.org/) CLI or VS Code extension
- Git

### Build and flash

```bash
git clone https://github.com/MaksimShchuplov/firelamp.git
cd firelamp
pio run -e esp32s3 -t upload
pio device monitor --baud 115200
```

The firmware version is set automatically to the short git SHA at build time.

### First boot

1. The lamp starts a Wi-Fi AP: **FireLamp-Setup**
2. Connect to it and open **192.168.4.1**
3. Enter your Wi-Fi credentials
4. The lamp reboots and is available at **http://firelamp.local**

Credentials survive all future firmware updates.

---

## Web UI

Open `http://firelamp.local` (or the IP shown in the serial monitor).

| Control | Range | Description |
|---------|-------|-------------|
| Brightness | 0–100 | Overall output with γ 2.2 curve |
| Contrast | 0–100 | Palette shift — low = yellows/white, high = deep reds |
| Cooling | 20–150 | Heat dissipation rate — lower = taller flames |
| Sparking | 0–255 | Ignition rate at the base |
| Blend | 0–255 | Temporal smoothing — lower = frozen glow |
| Theme | Fire / Ember / Plasma / Ice | Color palette |
| Presets | 8 slots | Tap to load · Long-press to save or delete · Export/Import as JSON |
| Surprise Me | — | Gemini AI effect — requires a key set in Settings |

All parameters persist to flash automatically.

---

## MQTT

Configure a broker in the settings modal (gear icon → MQTT tab).

**Home Assistant discovers the lamp automatically** — on connect the lamp publishes a retained [MQTT Discovery](https://www.home-assistant.io/integrations/mqtt/#mqtt-discovery) config, and a "Fire Lamp" light entity appears in Settings → Devices with on/off and brightness. No YAML required.

The lamp subscribes to **`<prefix>/set`** and publishes to **`<prefix>/state`** after every change.

**Payload fields (JSON):**

| Field | Type | Description |
|-------|------|-------------|
| `state` | `"ON"` / `"OFF"` | OFF saves brightness and sets to 0; ON restores it |
| `b` | 0–100 | Brightness |
| `c` | 0–100 | Contrast |
| `co` | 20–150 | Cooling |
| `sp` | 0–255 | Sparking |
| `bl` | 0–255 | Blend |
| `th` | 0–3 | Theme |

**Manual `configuration.yaml` example** (only needed if MQTT Discovery is disabled in HA):

```yaml
mqtt:
  light:
    - name: "Fire Lamp"
      state_topic: "firelamp/state"
      command_topic: "firelamp/set"
      brightness_state_topic: "firelamp/state"
      brightness_command_topic: "firelamp/set"
      brightness_value_template: "{{ value_json.b }}"
      brightness_command_template: '{"b": {{ value }} }'
      on_off_command_topic: "firelamp/set"
      payload_on: '{"state":"ON"}'
      payload_off: '{"state":"OFF"}'
      state_value_template: "{{ value_json.state }}"
```

---

## OTA Updates

Every push to `main` builds a new release. The UI checks automatically 8 seconds after Wi-Fi connects.

1. Tap **Check for Update** in the UI
2. Tap **Install Update** if a newer build is available
3. The LED strip shows a progress bar while the firmware downloads
4. The lamp reboots; the UI reloads automatically

### Recovery flash

If the OTA path to GitHub is broken, upload firmware directly from your browser:

1. Download `firmware.bin` from the [latest release](https://github.com/MaksimShchuplov/firelamp/releases/latest)
2. Open `http://firelamp.local/flash`
3. Select the file and click Upload

The file travels only over local Wi-Fi — no TLS to GitHub required.

---

## Architecture

See [ARCHITECTURE.md](ARCHITECTURE.md) for the dual-core task split, shared-state concurrency model (double-buffered palette, atomic parameter updates), OTA flow, PWA service worker strategy, and NVS partition layout.

---

## Configuration

All tunable constants are in [`src/config.h`](src/config.h):

```c
#define LED_PIN     5       // data pin
#define COLUMNS     20      // strip width
#define ROWS        40      // strip height
#define PSU_VOLTS   5
#define PSU_MAX_MA  20000   // FastLED power budget
```

---

## License

MIT — see [LICENSE](LICENSE).

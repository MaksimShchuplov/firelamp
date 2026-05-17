# Fire Lamp — AI Agent Context

ESP32-S3 firmware for an 800-LED WS2812B fire-effect lamp (20 columns × 40 rows).
Web UI served from PROGMEM over WiFi. OTA updates from GitHub Releases.

## Build & Flash

```bash
pio run -e esp32s3                        # compile
pio run -e esp32s3 -t upload              # flash via USB
pio device monitor --baud 115200          # serial output
pio run -e esp32s3 -t upload && pio device monitor   # flash + monitor
```

`FIRMWARE_VERSION` is injected automatically from `git rev-parse --short HEAD` by `get_version.py`.

## Project Structure

```
src/
  config.h      — all #define constants (strip geometry, fire tuning, network)
  globals.h     — extern declarations + function signatures shared across modules
  page.h        — PROGMEM HTML/CSS/JS UI blob (edit only for UI changes)
  fire.cpp      — palette, brightness/gamma, wind, fire simulation
  boot.cpp      — crash-counter boot-loop detection + OTA rollback
  network.cpp   — WiFiManager, all HTTP handlers, OTA update logic
  main.cpp      — global definitions, setup(), loop()
partitions_ota_4mb.csv   — custom partition table (two 1.75 MB OTA slots + NVS)
get_version.py           — PlatformIO pre-build script (injects git SHA as version)
.github/workflows/build.yml  — CI: build → version.json + MD5 → publish to Releases
```

## Architecture

### Dual-core split
- **Core 0** — `LEDTask` (pinned): `updateWind()` → `fireEffect()` → `FastLED.show()`, loops with 1 ms yield.
- **Core 1** — Arduino `loop()`: `serviceNetwork()` polls the web server and deferred NVS writes.

### Shared-state concurrency
UI parameters (`uiBright`, `uiContrast`, `uiCooling`, `uiSparking`, `uiBlend`, `uiTheme`, `appliedRaw`, `currentPowerW`, `updatePending`) and `coolMax[ROWS]` are `volatile` — written from Core 1 (web handlers / `recalcCooling`), read from Core 0. Single-byte writes are atomic on Xtensa LX7; `volatile` prevents compiler register-caching across the task boundary.

`heatPalette` uses a **double-buffer + atomic index flip**: `buildHeatPalette()` writes into `heatPalette[1 - activePal]`, then sets `activePal` in a single byte write. `fireEffect()` snapshots `activePal` once at the start of each frame so a mid-frame flip cannot split palette reads.

### WiFi & provisioning
Credentials are **never compiled into the binary**. On first boot the lamp starts AP `FireLamp-Setup` (no password); the user connects and opens `192.168.4.1` to enter credentials via WiFiManager captive portal. Credentials are stored by the ESP32 WiFi driver in the `nvs` flash partition, which OTA never touches — they survive all firmware updates.

The lamp is accessible as `http://firelamp.local` (mDNS) and as `firelamp` in the router DHCP table.

### OTA update flow
1. On WiFi connect, `autoUpdateCheck` task fires after 8 s, fetches `version.json`, sets `updatePending` flag. Browser sees `"upd":1` in `/state` and shows a silent badge.
2. Browser calls `/checkupdate` → ESP fetches `version.json` (cached 60 s) and compares SHA against `FIRMWARE_VERSION`.
3. Browser calls `/update` (with `X-Requested-With: firelamp` CSRF header) → ESP sets MD5 from `version.json`, sends HTTP 200, closes the connection, then calls `httpUpdate.update()` synchronously. After success the ESP reboots automatically. UI polls `/info` every 3 s until lamp responds, then auto-reloads.
4. `boot.cpp` counts consecutive hard crashes (panic/watchdog). On the third consecutive crash it calls `Update.rollBack()` + restart, reverting to the previous OTA slot.

### NVS persistence
UI parameters (`bright2`, `contrast`, `cooling`, `sparking`, `blend`, `theme`) are written to the `lamp` NVS namespace after 2.5 s of inactivity (`NVS_COMMIT_DELAY_MS`) to avoid flash wear from slider dragging.

The boot-loop crash counter uses a separate `boot` NVS namespace so it never shares an open `Preferences` handle with the UI params.

### Flash partition layout
| Partition | Size | Purpose |
|-----------|------|---------|
| nvs | 20 KB | WiFi credentials, lamp settings, crash counter |
| app0 / app1 | 1.75 MB each | OTA dual-bank |
| coredump | 64 KB | Post-mortem crash dumps |
| nvs2 | 384 KB | Reserved NVS space |

### CI
Every push to `main` builds the firmware, generates `version.json` (git short-SHA + MD5), publishes a versioned release tagged `build-<sha>` with auto-generated changelog, and updates the rolling `latest` release. The OTA endpoint always points to `latest`.

## Key Constants (src/config.h)

- `COLUMNS 20`, `ROWS 40`, `NUM_LEDS 800`
- `BLEND_DEFAULT 50` — default temporal smoothing; runtime value is `uiBlend` (0 = freeze, 255 = instant)
- `THEME_DEFAULT 0` — default color theme; 0=Fire 1=Ember 2=Plasma 3=Ice; runtime value is `uiTheme`
- `SPARK_INTENSITY 240` — max heat added per spark
- `BRIGHT_GAMMA 2.2` — perceptual brightness curve
- `PSU_VOLTS 5`, `PSU_MAX_MA 20000` — FastLED power limiter (intentionally above PSU rating)
- `WIFI_PORTAL_TIMEOUT_S 120` — if not configured in 2 min, fire runs without WiFi
- `MDNS_NAME "firelamp"`

## HTTP API (src/network.cpp)

All state-mutating endpoints require header `X-Requested-With: firelamp` (CSRF).

| Endpoint | Params | Notes |
|----------|--------|-------|
| `GET /state` | — | JSON: `b,c,co,sp,w,bl,th,upd` |
| `GET /setb` | `v=0..100` | brightness |
| `GET /setc` | `v=0..100` | contrast |
| `GET /setco` | `v=20..150` | cooling |
| `GET /setsp` | `v=0..255` | sparking |
| `GET /setbl` | `v=0..255` | blend |
| `GET /settheme` | `v=0..3` | color theme; calls `buildHeatPalette()` |
| `GET /reset` | — | restore all defaults |
| `GET /checkupdate` | — | compare version (60 s cache) |
| `GET /update` | — | start OTA; ESP reboots on success |
| `GET /info` | — | flash_mb, free_heap, ip, version, build |
| `GET /resetwifi` | — | clear credentials + reboot |

## Hardware Notes

- **PSU**: Mean Well 60 W (5 V 12 A). FastLED power limiter set to 20 A (100 W) — intentionally above PSU rating to allow full brightness, relying on voltage drop along the 800-LED strip to keep real current below 12 A.
- **FPS ceiling**: ~40 FPS is the WS2812B protocol limit for 800 LEDs on one data line. The RMT5 driver on S3 overlaps frame computation with LED transmission.
- **Upload**: `--no-stub` flag required, speed 57600 (native USB CDC disabled, `ARDUINO_USB_MODE=0`).

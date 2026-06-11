# FireLamp — Hardware Assembly Guide

## Bill of Materials

| Qty | Part | Spec | Notes |
|-----|------|------|-------|
| 1 | ESP32-S3 DevKitC-1 | N8R2 or N16R8 | Any S3 board works; N8R2 is the cheapest |
| 1 | WS2812B LED strip | 144 LED/m, 5 V, IP30 | Cut to exactly 800 LEDs (5.56 m at 144/m) |
| 1 | 5 V PSU | ≥ 10 A (50 W) | Mean Well LRS-60-5 (12 A) recommended |
| 1 | Resistor | 300–500 Ω, ¼ W | Series protection on data line |
| 1 | Capacitor | 1000 µF / 10 V | Across strip power rails, near the first LED |
| — | Wire, red | 18 AWG, ≥ 30 cm | PSU + to strip 5 V |
| — | Wire, black | 18 AWG, ≥ 30 cm | PSU GND to strip GND and ESP GND |
| — | Wire, any color | 24 AWG, ≥ 30 cm | Data line (ESP GPIO 5 → strip DIN) |
| 1 | Cylindrical frame | 125 mm diameter, ≥ 45 cm tall | Acrylic tube, PVC pipe, or 3D-printed ring stack |
| — | Diffuser film | white or frosted | Wraps the outside of the frame to blend pixels |

> **Strip selection:** use genuine WS2812B, not WS2811, SK6812, or "NeoPixel compatible" variants unless you verify the 3.3 V logic level is accepted. Most WS2812B chips tolerate 3.3 V data on short runs (< 50 cm). Add a 74AHCT125 level shifter for longer data runs.

---

## Power

800 LEDs at full white draw up to **48 W** (800 × 60 mA at 5 V). The default firmware cap is set generously high (`PSU_MAX_MA 20000` in `config.h`) to rely on voltage drop rather than software limiting — real draw on typical fire effects is 8–15 W.

**Minimum PSU:** 5 V / 10 A (50 W).  
**Recommended:** Mean Well LRS-60-5 (5 V / 12 A, 60 W) — fanless, compact, reliable.

### Power injection

For 800 LEDs the voltage drop along a single wire can dim the far end. Inject power at **both ends** of the strip:

```
PSU 5 V ──── start of strip (LED 1) ───┐
             (LEDs 1–400)              │ strip runs down
             (LEDs 401–800)            │
PSU 5 V ──── end of strip  (LED 800) ──┘
```

Both injection points share the same PSU GND rail. Use 18 AWG or thicker wire for the power runs.

---

## Wiring

```
                       300 Ω
ESP32-S3 GPIO 5 ───[===]─── Strip DIN (data in)
ESP32-S3 GND    ─────────── Strip GND
                            │
                         [1000 µF]  ← place across strip rails near LED 1
                            │
PSU 5 V ────────────────── Strip 5 V
PSU GND ────────────────── Strip GND ─── ESP32-S3 GND
```

**PSU earth:** connect the PSU earth (green/yellow) to the metal enclosure if used.

> **Do not power the ESP32-S3 from the strip's 5 V rail.** Noise from 800 LEDs switching can cause random resets. Power the ESP from USB or a separate small regulator.

---

## LED Matrix Layout

The firmware assumes a **top-to-bottom, left-to-right** scan with **no serpentine**:

```
LED 0 ──→ LED 19      ← top row (y = 39, flame tip)
LED 20 ──→ LED 39
...
LED 780 ──→ LED 799   ← bottom row (y = 0, heat source)
```

If your strip winds serpentine (even rows reversed), the fire will look mirrored on every other row. Fix by rewiring the strip in a single-direction scan, or add a software de-serpentine transform in `fireEffect()`.

### Physical mounting

1. Cut the strip into 40 rows of 20 LEDs each.
2. Mount rows horizontally around the cylinder from top to bottom, keeping all arrows pointing the same direction (no reversal).
3. Solder short jumper wires between rows — keep the data jumpers < 5 cm to avoid signal ringing.
4. Connect 5 V and GND injection wires at top and bottom as described above.

---

## Assembly Steps

1. **Test the strip before mounting.** Flash the firmware with `COLOR_TEST 1` in `config.h`. The strip should show solid red. Green or blue means wrong `LED_COLOR_ORDER` — change `GRB` → `RGB` or `BRG` as needed. Set `COLOR_TEST` back to `0` when done.

2. **Add the decoupling capacitor.** Solder the 1000 µF cap across the 5 V and GND pads at the very start of the strip (LED 0 side). Observe polarity.

3. **Add the data resistor.** Solder the 300–500 Ω resistor in series on the data wire between GPIO 5 and the strip DIN pad. Place it near the ESP, not the strip.

4. **Wire GND first.** Connect ESP GND → strip GND → PSU GND before connecting 5 V to avoid floating signals.

5. **Power on without the ESP connected.** Verify PSU output is 5.0–5.2 V with a multimeter before connecting the ESP.

6. **Flash and test.** Connect the ESP via USB, flash the firmware (`pio run -e esp32s3 -t upload`), and check `http://firelamp.local` once the lamp has joined Wi-Fi.

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---------|-------------|-----|
| Strip shows wrong color (green instead of red on color test) | `LED_COLOR_ORDER` mismatch | Change `GRB` → `RGB` or `BRG` in `config.h` |
| Strip flickers or resets randomly | ESP powered from strip 5 V rail, or no decoupling cap | Power ESP from USB; add 1000 µF cap at strip start |
| Far end of strip is dim or yellowish | Voltage drop on long power run | Inject power at both ends of the strip |
| First few LEDs are correct, rest are garbage | Data line too long or no series resistor | Add 300–500 Ω resistor near the ESP; keep data wire < 50 cm |
| Fire pattern is mirrored on every other row | Strip wired serpentine | Re-solder rows in single-direction scan |
| ESP not detected by computer | Native USB disabled in firmware | Use `--no-stub` upload flag; set speed to 57600 (already in `platformio.ini`) |
| `http://firelamp.local` doesn't resolve | mDNS not working on your network | Use the IP address from the serial monitor instead |
| OTA update fails immediately | GitHub CDN unreachable | Use the manual `/flash` recovery endpoint |
| Lamp reboots three times then shows different firmware | Crash loop triggered rollback | Check serial log for panic address; roll forward with a new build |

---

## Pin Reference

| ESP32-S3 pin | Connected to | Notes |
|--------------|-------------|-------|
| GPIO 5 | Strip DIN (via 300–500 Ω) | Defined as `LED_PIN` in `config.h` |
| GND | Strip GND, PSU GND | Common ground — all three must be connected |
| 5 V (VBUS, USB) | — | Do not connect to strip 5 V |

To use a different data pin, change `#define LED_PIN` in `src/config.h` and reflash.

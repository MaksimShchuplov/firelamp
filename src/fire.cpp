#include <Arduino.h>
#include <FastLED.h>
#include "globals.h"

// =============================================================================
//  PALETTE — heat LUT (red → orange → white-hot), double-buffered
// =============================================================================

// Called from Core 1 only (network handlers / startNetwork). Not reentrant.
// Double-buffer + memw barrier make the palette flip safe to read from Core 0.
void buildHeatPalette() {
    const uint8_t next  = 1 - activePal;
    const float   power = 1.0f + ((uiContrast - 50.0f) / 50.0f);
    const uint8_t theme = uiTheme;            // snapshot volatile once
    for (int i = 0; i < 256; i++) {
        float    n    = powf((float)i / 255.0f, power);
        uint16_t m    = (uint16_t)(n * 255.0f);
        uint8_t  t192 = (uint8_t)((m * 191) / 255);
        uint8_t  ramp = (uint8_t)((t192 & 0x3F) << 2);
        switch (theme) {
            case 1: // Ember — deep reds, no white-hot
                if      (t192 > 0x80) heatPalette[next][i] = CRGB(255, ramp >> 1, 0);
                else if (t192 > 0x40) heatPalette[next][i] = CRGB(ramp, 0, 0);
                else                  heatPalette[next][i] = CRGB(ramp >> 2, 0, 0);
                break;
            case 2: // Plasma — purple → magenta → white
                if      (t192 > 0x80) heatPalette[next][i] = CRGB(255, ramp, 255);
                else if (t192 > 0x40) heatPalette[next][i] = CRGB(ramp, 0, 255);
                else                  heatPalette[next][i] = CRGB(ramp >> 2, 0, ramp);
                break;
            case 3: // Ice — dark blue → cyan → white
                if      (t192 > 0x80) heatPalette[next][i] = CRGB(ramp, 255, 255);
                else if (t192 > 0x40) heatPalette[next][i] = CRGB(0, ramp, 255);
                else                  heatPalette[next][i] = CRGB(0, ramp >> 2, ramp);
                break;
            default: // Fire — classic red → orange → yellow → white
                if      (t192 > 0x80) heatPalette[next][i] = CRGB(255, 255, ramp);
                else if (t192 > 0x40) heatPalette[next][i] = CRGB(255, ramp, 0);
                else                  heatPalette[next][i] = CRGB(ramp, 0, 0);
        }
    }
    // seq_cst store generates a full memory barrier (memw on LX7) automatically,
    // ensuring all palette stores above are visible to Core 0 before the index
    // flip. The explicit asm is no longer needed.
    activePal.store(next, std::memory_order_seq_cst);
}

// =============================================================================
//  BRIGHTNESS
// =============================================================================

void applyBrightness() {
    uint8_t raw;
    if (uiBright == 0) {
        raw = 0;
    } else {
        int v = (int)(powf((float)uiBright / 100.0f, BRIGHT_GAMMA) * 255.0f + 0.5f);
        raw = (uint8_t)(v < BRIGHT_FLOOR ? BRIGHT_FLOOR : v);
    }
    if (raw != appliedRaw) {
        appliedRaw = raw;
        FastLED.setBrightness(raw);
        FastLED.setDither(raw < BRIGHT_DITHER_ON ? DISABLE_DITHER : BINARY_DITHER);
    }
}

void setBright(int v) {
    v = constrain(v, 0, 100);
    if ((uint8_t)v != uiBright) {
        uiBright   = (uint8_t)v;
        prefsDirty = true;
        prefsTouch = millis();
        // applyBrightness() is called at the top of fireEffect() on Core 0,
        // so the change takes effect within one frame (~25 ms) without
        // crossing core boundaries into FastLED's global state.
    }
}

// =============================================================================
//  FIRE SIMULATION
// =============================================================================

void updateWind() {
    if (millis() - lastWindChange > WIND_CHANGE_INTERVAL) {
        for (int y = 0; y < ROWS; y++)
            windTarget[y] = (random8(100) < WIND_PULSE_PROBABILITY)
                ? (float)((int)random8(2 * WIND_MAX_STRENGTH + 1) - WIND_MAX_STRENGTH)
                : 0.0f;
        lastWindChange = millis();
    }
    for (int y = 0; y < ROWS; y++)
        windDir[y] += (windTarget[y] - windDir[y]) * 0.1f;
}

void recalcCooling() {
    const uint8_t cooling = uiCooling;   // snapshot volatile once — prevents lo/hi split if Core 1 updates mid-loop
    const uint8_t lo = (cooling > 10) ? cooling - 10 : 0;
    for (int y = 0; y < ROWS; y++)
        coolMax[y] = (uint8_t)((random8(lo, cooling + 10) * 10) / ROWS + 2);
}

void updatePowerCalc() {
    float req = (float)calculate_unscaled_power_mW(leds, NUM_LEDS)
                * ((float)appliedRaw / 255.0f) / 1000.0f;
    float cap = ((float)PSU_VOLTS * (float)PSU_MAX_MA) / 1000.0f;
    float w   = (req > cap) ? cap : req;
    currentPowerMw = (uint32_t)(w * 1000.0f);  // atomic uint32 write; avoids torn float read on Core 1
    lastPowerCalc = millis();
}

void fireEffect() {
    applyBrightness();  // runs on Core 0, safe alongside FastLED.show()

    // 1. Cooling
    for (int y = 0; y < ROWS; y++) {
        const uint8_t cmax = coolMax[y];
        for (int x = 0; x < COLUMNS; x++)
            heat[y][x] = qsub8(heat[y][x], random8(cmax));
    }

    // 2. Upward convection with wind
    for (int y = ROWS - 1; y > 0; y--) {
        const int wind = (int)lroundf(windDir[y]);
        const int y1   = y - 1, y2 = (y >= 2) ? y - 2 : 0;
        for (int x = 0; x < COLUMNS; x++) {
            int nx = x + wind;
            if (nx >= COLUMNS) nx -= COLUMNS;
            else if (nx < 0)   nx += COLUMNS;
            // *3/5 + *2/5 without division in the inner loop
            heat[y][x] = (uint8_t)(((uint16_t)heat[y1][nx] * 153
                                  + (uint16_t)heat[y2][nx] * 102) >> 8);
        }
    }

    // 3. Ignite sparks at the base
    for (int x = 0; x < COLUMNS; x++) {
        if (random8() < uiSparking) {
            uint8_t y = random8(3);
            heat[y][x] = qadd8(heat[y][x], random8(SPARK_INTENSITY - 40, SPARK_INTENSITY));
        }
    }

    // 4. Render with temporal blend.
    // Snapshot activePal once — a mid-frame flip must not split palette reads.
    const CRGB *pal = heatPalette[activePal & 1];  // & 1 guards against memory corruption setting it > 1
    for (int y = 0; y < ROWS; y++) {
        const uint16_t base = (uint16_t)(ROWS - 1 - y) * COLUMNS;
        for (int x = 0; x < COLUMNS; x++)
            nblend(leds[base + x], pal[heat[y][x]], uiBlend);
    }

    if (millis() - lastPowerCalc > POWER_CALC_INTERVAL_MS)
        updatePowerCalc();
}

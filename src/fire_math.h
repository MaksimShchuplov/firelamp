#pragma once
#include <cmath>
#include <cstdint>
#include "config.h"

// Pure fire/brightness math, free of Arduino and FastLED so the native tests
// exercise the shipped expressions instead of re-implementing them.
// Same header-only pattern as text_utils.h / ota_utils.h / mqtt_state.h.

struct Rgb8 { uint8_t r, g, b; };

// Contrast 0..100 → gamma exponent. At 50 this is 1.0 (linear).
inline float palettePower(uint8_t contrast) {
    return 1.0f + (((float)contrast - 50.0f) / 50.0f);
}

// i==0 must stay black: at contrast 0 power is 0 and powf(0,0)==1, which would
// map "no heat" to the brightest palette entry.
inline float paletteNorm(float power, int i) {
    return (i == 0) ? 0.0f : powf((float)i / 255.0f, power);
}

// Both branch boundaries use `>` DELIBERATELY. Do not "normalise" them to bit
// tests, and do not make them match FastLED's HeatColor, which this palette is
// derived from. This has been changed and reverted once already.
//
// `ramp` wraps to 0 at every multiple of 0x40, so a `>` comparison sends the
// exact boundary value one branch too low with ramp==0 — collapsing that entry
// to the branch's zero-intensity colour. At contrast 50 that is palette index
// 86 (black in every theme) and 171-172 (pure red in Fire).
//
// That is a discontinuity in the LUT, and it is exactly what the lamp's tuned
// look depends on. It is NOT equivalent to randomly darkening cells: the
// affected entries are selected by heat VALUE, and the heat field is spatially
// smooth, so they land along an iso-thermal contour. Measured on the real
// model (defaults, 1000 frames): ~2.3 cells/frame hit index 86, present in 81%
// of frames, and 16% of them have an adjacent hit versus ~2% expected from a
// uniform scatter — 8x more clustered than noise.
//
// That clustering is why the effect survives at all. The temporal nblend and
// the vertical convection blend are both low-pass filters: per-cell white
// noise (like the random cooling in fireEffect) averages away invisibly, while
// a contour that drifts continuously with the flame does not. The result reads
// as thin dark filaments moving through the fire — visually, soot streaks.
//
// Replacing this with per-cell "soot" in the simulation would be a regression:
// uniformly scattered random cells are precisely the white noise the filters
// erase. Reproducing it properly would need spatially-correlated drifting
// noise, which is more machinery and a different look (coarse slow patches
// instead of fine filaments).
inline Rgb8 heatRamp(uint8_t theme, uint8_t t192) {
    const uint8_t ramp = (uint8_t)((t192 & 0x3F) << 2);
    switch (theme) {
        case 1: // Ember — deep reds, no white-hot
            if (t192 > 0x80) return {255, (uint8_t)(ramp >> 1), 0};
            if (t192 > 0x40) return {ramp, 0, 0};
            return {(uint8_t)(ramp >> 2), 0, 0};
        case 2: // Plasma — purple → magenta → white
            if (t192 > 0x80) return {255, ramp, 255};
            if (t192 > 0x40) return {ramp, 0, 255};
            return {(uint8_t)(ramp >> 2), 0, ramp};
        case 3: // Ice — dark blue → cyan → white
            if (t192 > 0x80) return {ramp, 255, 255};
            if (t192 > 0x40) return {0, ramp, 255};
            return {0, (uint8_t)(ramp >> 2), ramp};
        default: // Fire — red → orange → yellow → white
            if (t192 > 0x80) return {255, 255, ramp};
            if (t192 > 0x40) return {255, ramp, 0};
            return {ramp, 0, 0};
    }
}

// Perceptual brightness curve. Pure, so either core may call it; appliedRaw is
// written only by Core 0, so Core-1 callers derive the value instead of reading
// that atomic.
inline uint8_t brightToRaw(uint8_t bright) {
    if (bright == 0) return 0;
    int v = (int)(powf((float)bright / 100.0f, BRIGHT_GAMMA) * 255.0f + 0.5f);
    return (uint8_t)(v < BRIGHT_FLOOR ? BRIGHT_FLOOR : v);
}

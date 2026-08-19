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

// The two branch boundaries are DELIBERATELY ASYMMETRIC — do not "normalise"
// them to match each other or to FastLED's HeatColor.
//
// `ramp` wraps to 0 at every multiple of 0x40, so a `>` comparison sends the
// exact boundary value one branch too low with ramp==0, collapsing that entry
// to the branch's zero-intensity colour. That is a discontinuity in the LUT,
// but it is not automatically a defect: the lamp's tuned look was dialled in
// against it.
//
//   Upper boundary (0x80) — BIT TEST. The `>` form put t192==0x80 in the mid
//   branch, giving pure red between two yellows (palette 171-172 at contrast
//   50). Those red flecks in the hottest band were not wanted, so this one is
//   smoothed.
//
//   Lower boundary (0x40) — `>` KEPT ON PURPOSE. This drops t192==0x40 into
//   the low branch with ramp==0, i.e. black (palette 86 at contrast 50). The
//   resulting dark speckling through the mid-flame is what reads as texture
//   and contrast on the real lamp; removing it made the fire look flat. Tested
//   on hardware and chosen deliberately — see test_heat_ramp_lower_seam_*.
inline Rgb8 heatRamp(uint8_t theme, uint8_t t192) {
    const uint8_t ramp = (uint8_t)((t192 & 0x3F) << 2);
    switch (theme) {
        case 1: // Ember — deep reds, no white-hot
            if (t192 & 0x80) return {255, (uint8_t)(ramp >> 1), 0};
            if (t192 > 0x40) return {ramp, 0, 0};
            return {(uint8_t)(ramp >> 2), 0, 0};
        case 2: // Plasma — purple → magenta → white
            if (t192 & 0x80) return {255, ramp, 255};
            if (t192 > 0x40) return {ramp, 0, 255};
            return {(uint8_t)(ramp >> 2), 0, ramp};
        case 3: // Ice — dark blue → cyan → white
            if (t192 & 0x80) return {ramp, 255, 255};
            if (t192 > 0x40) return {0, ramp, 255};
            return {0, (uint8_t)(ramp >> 2), ramp};
        default: // Fire — red → orange → yellow → white
            if (t192 & 0x80) return {255, 255, ramp};
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

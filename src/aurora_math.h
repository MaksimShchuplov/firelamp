#pragma once
// Pure aurora math, shared by aurora.cpp and the native tests.
#include <cmath>
#include <cstdint>
#include "config.h"
#include "fire_math.h"
#include "lava_math.h"   // lavaSmooth, lavaMix

// sky: the night behind; fringe: thin tint at the lower hem; body: the
// curtain's main colour; top: what it fades into higher up.
struct AuroraPal { Rgb8 sky, fringe, body, top; };

// Per theme. The default is the classic oxygen-green curtain with a magenta
// fringe and violet top; the others follow the lamp's themes.
inline AuroraPal auroraPalette(uint8_t theme) {
    switch (theme) {
        case 1:  return {{8, 1, 3},  {255, 120, 0},  {200, 20, 0},  {120, 0, 40}};   // Ember: rare red aurora
        case 2:  return {{6, 1, 14}, {80, 120, 255}, {200, 30, 220}, {80, 0, 180}};   // Plasma: violet-pink
        case 3:  return {{1, 4, 16}, {170, 255, 255}, {0, 200, 190}, {20, 60, 210}};  // Ice: teal-blue
        default: return {{1, 4, 14}, {230, 40, 140}, {30, 230, 90},  {110, 30, 200}}; // Fire theme: classic
    }
}

// Curtain density for column x at time t (s), 0..1. Three waves travel round
// the tube at different speeds and directions; the smoothstep turns their sum
// into curtains with open sky between them.
inline float auroraCurtain(float x, float t) {
    const float k = 2.0f * (float)M_PI / COLUMNS;
    const float v = 0.55f * (0.5f + 0.5f * sinf(1.0f * k * x + 0.21f * t))
                  + 0.30f * (0.5f + 0.5f * sinf(2.0f * k * x - 0.13f * t + 1.3f))
                  + 0.15f * (0.5f + 0.5f * sinf(3.0f * k * x + 0.37f * t + 2.1f));
    return lavaSmooth(AURORA_GAP_LO, AURORA_GAP_HI, v);
}

// Height of the lower hem for column x, in rows from the base.
inline float auroraHem(float x, float t) {
    const float k = 2.0f * (float)M_PI / COLUMNS;
    return AURORA_HEM_Y + AURORA_HEM_AMP * (0.6f * sinf(2.0f * k * x + 0.17f * t)
                                          + 0.4f * sinf(5.0f * k * x - 0.29f * t + 0.7f));
}

// Vertical rays: each column shimmers at its own rate, 1 - depth .. 1.
inline float auroraRay(int x, float t) {
    const float rate = 0.9f + 0.6f * (float)((x * 37) % 11) / 10.0f;
    return 1.0f - AURORA_RAY_DEPTH * (0.5f + 0.5f * sinf(rate * t + (float)x * 2.4f));
}

// Light profile above and below the hem: a fast fade below, a long one above.
inline float auroraVertical(float y, float hem) {
    const float d = y - hem;
    return d < 0.0f ? expf(d / AURORA_HEM_SOFT) : expf(-d / AURORA_RAY_LEN);
}

inline uint8_t auroraAdd8(uint8_t a, float b) {
    const float v = (float)a + b;
    return (uint8_t)(v > 255.0f ? 255.0f : v + 0.5f);
}

// Colour of one pixel: the sky plus curtain light whose tint runs from the
// fringe at the hem through the body to the top colour higher up.
inline Rgb8 auroraColor(const AuroraPal &p, float curtain, float ray, float pulse, float y, float hem) {
    const float d     = y - hem;
    const float light = AURORA_GAIN * curtain * ray * pulse * auroraVertical(y, hem);
    const Rgb8  upper = lavaMix(p.body, p.top, lavaSmooth(0.0f, AURORA_RAY_LEN * 1.5f, d));
    const Rgb8  c     = lavaMix(upper, p.fringe, 0.6f * expf(-fabsf(d) / AURORA_FRINGE));
    return {auroraAdd8(p.sky.r, c.r * light), auroraAdd8(p.sky.g, c.g * light), auroraAdd8(p.sky.b, c.b * light)};
}

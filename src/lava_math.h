#pragma once
// Pure lava-lamp math, shared by lava.cpp and the native tests.
#include <cmath>
#include <cstdint>
#include "config.h"
#include "fire_math.h"

// x in columns [0, COLUMNS), y in rows counted from the base, r in rows.
struct LavaBlob   { float x, y, r; };
struct LavaPal    { Rgb8 liquid, wax, core; };
struct LavaMotion { float x, vx, phase, speed, r, breathe; };

// Advances one blob by dt seconds and returns where it is now.
inline LavaBlob lavaStep(LavaMotion &m, float dt) {
    m.phase   += m.speed * dt;
    m.breathe += LAVA_BREATHE_RATE * dt;
    m.x = fmodf(m.x + m.vx * dt + (float)COLUMNS, (float)COLUMNS);
    // (1 - cos)/2 travel lingers at the base and the top, like wax that warms
    // before it rises and cools before it sinks.
    return {m.x, (float)(ROWS - 1) * 0.5f * (1.0f - cosf(m.phase)),
            m.r * (1.0f + LAVA_BREATHE_DEPTH * sinf(m.breathe))};
}

// Horizontal distance on the cylinder, the short way round, in row units.
inline float lavaDx(float a, float b) {
    float d = fabsf(a - b);
    if (d > COLUMNS * 0.5f) d = COLUMNS - d;
    return d * LAVA_ASPECT;
}

// Ripple on the bottom pool's surface for column x at time t (s). Depends only
// on the column, so the renderer computes it once per column per frame.
inline float lavaWobble(float x, float t) {
    const float k = 2.0f * (float)M_PI / COLUMNS;   // whole waves around the tube: no seam
    return 0.45f * sinf(3.0f * k * x + 0.35f * t) + 0.25f * sinf(5.0f * k * x - 0.22f * t);
}

inline float lavaField(const LavaBlob *b, int n, float x, float y, float wobble) {
    const float q = LAVA_POOL_H / (y + 1.0f + wobble);   // y >= 0 and |wobble| <= 0.7: denominator > 0.3
    float f = q * q;
    for (int i = 0; i < n; i++) {
        const float dx = lavaDx(x, b[i].x), dy = y - b[i].y;
        f += (b[i].r * b[i].r) / (dx * dx + dy * dy + 0.05f);
    }
    return f;
}

inline float lavaSmooth(float e0, float e1, float v) {
    float t = (v - e0) / (e1 - e0);
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return t * t * (3.0f - 2.0f * t);
}

inline uint8_t lavaMix8(uint8_t a, uint8_t b, float t) {
    return (uint8_t)((float)a + ((float)b - (float)a) * t + 0.5f);
}

inline Rgb8 lavaMix(Rgb8 a, Rgb8 b, float t) {
    return {lavaMix8(a.r, b.r, t), lavaMix8(a.g, b.g, t), lavaMix8(a.b, b.b, t)};
}

inline LavaPal lavaPalette(uint8_t theme) {
    const Rgb8  d = heatRamp(theme, LAVA_T_BG);
    const float l = LAVA_BG_LEVEL / 255.0f;
    return {{lavaMix8(0, d.r, l), lavaMix8(0, d.g, l), lavaMix8(0, d.b, l)},
            heatRamp(theme, LAVA_T_WAX), heatRamp(theme, LAVA_T_CORE)};
}

// yNorm: 0 at the base, 1 at the top. The liquid is lit from below, as in a
// real lamp, so it fades to half brightness at the top.
inline Rgb8 lavaColor(const LavaPal &p, float field, float yNorm) {
    const Rgb8  liquid = lavaMix({0, 0, 0}, p.liquid, 1.0f - 0.5f * yNorm);
    const Rgb8  rim    = lavaMix({0, 0, 0}, p.wax, LAVA_RIM_SHADE);
    const Rgb8  body   = lavaMix(rim, p.wax, lavaSmooth(LAVA_EDGE_LO, LAVA_CORE_LO, field));
    const Rgb8  wax    = lavaMix(body, p.core, LAVA_CORE_MIX * lavaSmooth(LAVA_CORE_LO, LAVA_CORE_HI, field));
    return lavaMix(liquid, wax, lavaSmooth(LAVA_EDGE_LO, LAVA_EDGE_HI, field));
}

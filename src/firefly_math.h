#pragma once
// Pure firefly math, shared by fireflies.cpp and the native tests.
#include <cmath>
#include <cstdint>
#include "config.h"
#include "fire_math.h"
#include "lava_math.h"   // lavaDx, lavaMix

// x in columns [0, COLUMNS), y in rows from the base, heading in radians,
// phase in seconds within the current flash cycle.
struct Firefly  { float x, y, heading, speed, phase, period, flash; };
struct FlyPal   { Rgb8 nightTop, nightBase, glow, core; };

inline FlyPal flyPalette(uint8_t theme) {
    switch (theme) {
        case 1:  return {{6, 1, 0},  {30, 6, 0},  {255, 90, 0},   {255, 200, 80}};   // Ember: drifting sparks
        case 2:  return {{6, 0, 16}, {22, 4, 34}, {220, 40, 255}, {255, 190, 255}};  // Plasma: violet
        case 3:  return {{0, 4, 18}, {0, 16, 36}, {60, 170, 255}, {210, 240, 255}};  // Ice: cold blue
        default: return {{2, 4, 20}, {8, 26, 8},  {170, 255, 30}, {255, 255, 170}};  // Fire theme: summer fireflies
    }
}

// Flash envelope, 0..1: a quick smooth rise, a longer fade, then the ember.
inline float flyGlow(float phase, float flash) {
    if (phase >= flash) return FLY_EMBER;
    const float rise = FLY_RISE * flash;
    float g;
    if (phase < rise) { const float u = phase / rise; g = u * u * (3.0f - 2.0f * u); }
    else              { const float u = 1.0f - (phase - rise) / (flash - rise); g = u * u; }
    return FLY_EMBER + (1.0f - FLY_EMBER) * g;
}

// Advances one fly by dt. turn is a random number in [-1, 1] supplied by the
// caller, so the motion stays deterministic for tests. Flies bounce off the
// top and bottom; round the tube they simply wrap.
inline void flyStep(Firefly &f, float dt, float turn) {
    f.heading += FLY_TURN * turn * dt;
    f.x = fmodf(f.x + cosf(f.heading) * f.speed * dt / LAVA_ASPECT + (float)COLUMNS, (float)COLUMNS);
    f.y += sinf(f.heading) * f.speed * dt;
    if (f.y < FLY_Y_MIN) { f.y = FLY_Y_MIN; f.heading = -f.heading; }
    if (f.y > FLY_Y_MAX) { f.y = FLY_Y_MAX; f.heading = -f.heading; }
    f.phase += dt;
    if (f.phase >= f.period) f.phase -= f.period;
}

// Halo falloff at distance d (row units): 1 at the centre, 0.5 at FLY_HALO.
inline float flyHalo(float d) {
    const float q = (d * d) / (FLY_HALO * FLY_HALO);
    return 1.0f / (1.0f + q * q);
}

// Night background: dim sky fading into a faintly lit meadow at the base.
inline Rgb8 flyNight(const FlyPal &p, float yNorm) {
    return lavaMix(p.nightBase, p.nightTop, yNorm < 0.35f ? yNorm / 0.35f : 1.0f);
}

// Light from one fly at intensity g (0..1): tinted glow, whitening at the core.
inline Rgb8 flyLight(const FlyPal &p, float g) {
    const Rgb8 c = lavaMix(p.glow, p.core, g * g);
    return {(uint8_t)(c.r * g), (uint8_t)(c.g * g), (uint8_t)(c.b * g)};
}

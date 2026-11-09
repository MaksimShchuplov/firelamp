#include <Arduino.h>
#include <FastLED.h>
#include "globals.h"
#include "aurora_math.h"

// Called from fireEffect() on Core 0 only, so plain statics are safe here.
struct Star { uint8_t x, y; float rate, phase; };
static Star     s_stars[AURORA_STARS];
static bool     s_init = false;
static uint32_t s_last = 0;
static float    s_t    = 0.0f;

static void initStars() {
    for (int i = 0; i < AURORA_STARS; i++)
        s_stars[i] = {random8(COLUMNS), (uint8_t)random8(AURORA_STAR_MIN_Y, ROWS),
                      0.4f + (float)random8() / 255.0f, (float)random8() / 40.0f};
}

void auroraEffect() {
    const uint32_t now = millis();
    if (!s_init) { initStars(); s_init = true; s_last = now; }
    float dt = (float)(now - s_last) / 1000.0f;
    s_last = now;
    if (dt > AURORA_DT_MAX) dt = AURORA_DT_MAX;
    s_t += dt;

    float curtain[COLUMNS], hem[COLUMNS], ray[COLUMNS];
    for (int x = 0; x < COLUMNS; x++) {
        curtain[x] = auroraCurtain((float)x, s_t);
        hem[x]     = auroraHem((float)x, s_t);
        ray[x]     = auroraRay(x, s_t);
    }
    const float     pulse = 1.0f - AURORA_PULSE_DEPTH * (0.5f + 0.5f * sinf(0.3f * s_t));
    const AuroraPal pal   = auroraPalette(uiTheme.load(std::memory_order_relaxed));

    // Static, not on the stack: 2.4 KB would eat most of LEDTask's headroom.
    static CRGB frame[NUM_LEDS];
    for (int y = 0; y < ROWS; y++) {
        const uint16_t base = (uint16_t)(ROWS - 1 - y) * COLUMNS;
        for (int x = 0; x < COLUMNS; x++) {
            const Rgb8 c = auroraColor(pal, curtain[x], ray[x], pulse, (float)y, hem[x]);
            frame[base + x] = CRGB(c.r, c.g, c.b);
        }
    }
    // Stars twinkle in the open sky; under a curtain they are washed out anyway.
    for (const Star &s : s_stars) {
        const float tw = 0.5f + 0.5f * sinf(s.rate * s_t + s.phase);
        const uint8_t v = (uint8_t)(AURORA_STAR_LEVEL * tw * tw);
        frame[(ROWS - 1 - s.y) * COLUMNS + s.x] += CRGB(v, v, v);
    }
    for (int i = 0; i < NUM_LEDS; i++) nblend(leds[i], frame[i], AURORA_BLEND);
}

#include <Arduino.h>
#include <FastLED.h>
#include "globals.h"
#include "firefly_math.h"

// Called from fireEffect() on Core 0 only, so plain statics are safe here.
static Firefly  s_f[FLY_COUNT];
static bool     s_init = false;
static uint32_t s_last = 0;

static float frand(float lo, float hi) { return lo + (hi - lo) * ((float)random16() / 65535.0f); }

static void initFlies() {
    for (Firefly &f : s_f) {
        f = {frand(0.0f, (float)COLUMNS), frand(FLY_Y_MIN, FLY_Y_MAX), frand(0.0f, 2.0f * (float)M_PI),
             frand(FLY_SPEED_MIN, FLY_SPEED_MAX), 0.0f, frand(FLY_PERIOD_MIN, FLY_PERIOD_MAX),
             frand(FLY_FLASH_MIN, FLY_FLASH_MAX)};
        f.phase = frand(0.0f, f.period);
    }
}

void firefliesEffect() {
    const uint32_t now = millis();
    if (!s_init) { initFlies(); s_init = true; s_last = now; }
    float dt = (float)(now - s_last) / 1000.0f;
    s_last = now;
    if (dt > LAVA_DT_MAX) dt = LAVA_DT_MAX;

    const FlyPal pal = flyPalette(uiTheme.load(std::memory_order_relaxed));
    // Static, not on the stack: 2.4 KB would eat most of LEDTask's headroom.
    static CRGB frame[NUM_LEDS];
    for (int y = 0; y < ROWS; y++) {
        const Rgb8     n    = flyNight(pal, (float)y / (float)(ROWS - 1));
        const uint16_t base = (uint16_t)(ROWS - 1 - y) * COLUMNS;
        for (int x = 0; x < COLUMNS; x++) frame[base + x] = CRGB(n.r, n.g, n.b);
    }
    for (Firefly &f : s_f) {
        flyStep(f, dt, frand(-1.0f, 1.0f));
        const float g = flyGlow(f.phase, f.flash);
        const int   cx = (int)lroundf(f.x), cy = (int)lroundf(f.y);
        for (int dy = -FLY_REACH; dy <= FLY_REACH; dy++) {
            const int y = cy + dy;
            if (y < 0 || y >= ROWS) continue;
            for (int dxi = -FLY_REACH; dxi <= FLY_REACH; dxi++) {
                const int   x  = ((cx + dxi) % COLUMNS + COLUMNS) % COLUMNS;
                const float dx = lavaDx((float)x, f.x), ddy = (float)y - f.y;
                const Rgb8  c  = flyLight(pal, g * flyHalo(sqrtf(dx * dx + ddy * ddy)));
                frame[(ROWS - 1 - y) * COLUMNS + x] += CRGB(c.r, c.g, c.b);   // saturating add
            }
        }
    }
    for (int i = 0; i < NUM_LEDS; i++) nblend(leds[i], frame[i], FLY_BLEND);
}

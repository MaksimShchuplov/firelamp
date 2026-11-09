#include <Arduino.h>
#include <FastLED.h>
#include "globals.h"
#include "lava_math.h"

// Called from fireEffect() on Core 0 only, so plain statics are safe here.
static LavaMotion s_m[LAVA_BLOBS];
static bool       s_init = false;
static uint32_t   s_last = 0;
static float      s_t    = 0.0f;

static float frand(float lo, float hi) { return lo + (hi - lo) * ((float)random16() / 65535.0f); }

static void initBlobs() {
    for (int i = 0; i < LAVA_BLOBS; i++) {
        s_m[i] = {frand(0.0f, (float)COLUMNS), frand(-LAVA_DRIFT_MAX, LAVA_DRIFT_MAX),
                  frand(0.0f, 2.0f * (float)M_PI), frand(LAVA_SPEED_MIN, LAVA_SPEED_MAX),
                  frand(LAVA_R_MIN, LAVA_R_MAX), frand(0.0f, 2.0f * (float)M_PI)};
    }
}

void lavaEffect() {
    const uint32_t now = millis();
    if (!s_init) { initBlobs(); s_init = true; s_last = now; }
    float dt = (float)(now - s_last) / 1000.0f;
    s_last = now;
    if (dt > LAVA_DT_MAX) dt = LAVA_DT_MAX;
    s_t += dt;

    LavaBlob b[LAVA_BLOBS];
    for (int i = 0; i < LAVA_BLOBS; i++) b[i] = lavaStep(s_m[i], dt);

    float wobble[COLUMNS];
    for (int x = 0; x < COLUMNS; x++) wobble[x] = lavaWobble((float)x, s_t);

    const LavaPal pal = lavaPalette(uiTheme.load(std::memory_order_relaxed));
    for (int y = 0; y < ROWS; y++) {
        const uint16_t base  = (uint16_t)(ROWS - 1 - y) * COLUMNS;
        const float    yNorm = (float)y / (float)(ROWS - 1);
        for (int x = 0; x < COLUMNS; x++) {
            const Rgb8 c = lavaColor(pal, lavaField(b, LAVA_BLOBS, (float)x, (float)y, wobble[x]), yNorm);
            nblend(leds[base + x], CRGB(c.r, c.g, c.b), LAVA_BLEND);
        }
    }
}

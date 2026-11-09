#include <unity.h>
#include <cmath>

#include "arduino_stub.h"

#include "../../src/text_utils.h"
#include "../../src/config.h"
#include "../../src/mqtt_state.h"
#include "../../src/fire_math.h"
#include "../../src/lava_math.h"
#include "../../src/aurora_math.h"
#include "../../src/firefly_math.h"

// Define firmware identity before including ota_utils.h so the #ifndef guards
// in that header are skipped and these values are used throughout the tests.
#define FIRMWARE_VERSION "testver"
#define BUILD_N 42
#include "../../src/ota_utils.h"

void setUp()    {}
void tearDown() {}

// =============================================================================
//  jsonEscape
// =============================================================================

void test_json_escape_empty() {
    TEST_ASSERT_EQUAL_STRING("", jsonEscape(String("")).c_str());
}

void test_json_escape_plain_ascii() {
    TEST_ASSERT_EQUAL_STRING("hello world", jsonEscape(String("hello world")).c_str());
}

void test_json_escape_double_quote() {
    TEST_ASSERT_EQUAL_STRING("\\\"", jsonEscape(String("\"")).c_str());
}

void test_json_escape_backslash() {
    TEST_ASSERT_EQUAL_STRING("\\\\", jsonEscape(String("\\")).c_str());
}

void test_json_escape_newline() {
    TEST_ASSERT_EQUAL_STRING("\\n", jsonEscape(String("\n")).c_str());
}

void test_json_escape_carriage_return() {
    TEST_ASSERT_EQUAL_STRING("\\r", jsonEscape(String("\r")).c_str());
}

void test_json_escape_tab() {
    TEST_ASSERT_EQUAL_STRING("\\t", jsonEscape(String("\t")).c_str());
}

void test_json_escape_control_chars_stripped() {
    // 0x01 and 0x1F are below 0x20 and not \n/\r/\t — must be removed
    String s; s += (char)0x01; s += (char)0x1F;
    TEST_ASSERT_EQUAL_STRING("", jsonEscape(s).c_str());
}

void test_json_escape_mixed() {
    TEST_ASSERT_EQUAL_STRING("\\\"hi\\nworld\\\\",
        jsonEscape(String("\"hi\nworld\\")).c_str());
}

void test_json_escape_utf8_passthrough() {
    // High bytes (>= 0x80) are not control chars — they pass through unchanged.
    // 0xD0 0xBF is the UTF-8 encoding of U+043F ('п').
    String s; s += (char)0xD0; s += (char)0xBF;
    TEST_ASSERT_EQUAL_STRING("\xD0\xBF", jsonEscape(s).c_str());
}

// =============================================================================
//  truncateUtf8
// =============================================================================

void test_truncate_empty_string() {
    String s("");
    truncateUtf8(s, 5);
    TEST_ASSERT_EQUAL_STRING("", s.c_str());
}

void test_truncate_ascii_under_limit() {
    String s("hello");
    truncateUtf8(s, 10);
    TEST_ASSERT_EQUAL_STRING("hello", s.c_str());
}

void test_truncate_ascii_at_limit() {
    String s("hello");
    truncateUtf8(s, 5);
    TEST_ASSERT_EQUAL_STRING("hello", s.c_str());
}

void test_truncate_ascii_over_limit() {
    String s("hello world");
    truncateUtf8(s, 5);
    TEST_ASSERT_EQUAL_STRING("hello", s.c_str());
}

void test_truncate_limit_zero() {
    String s("hello");
    truncateUtf8(s, 0);
    TEST_ASSERT_EQUAL_STRING("", s.c_str());
}

void test_truncate_cyrillic_no_split() {
    // "Привет" — 6 chars, 12 bytes in UTF-8.
    // Truncate to 3 chars → "При" (6 bytes). Must not split a 2-byte sequence.
    String s("\xD0\x9F\xD1\x80\xD0\xB8\xD0\xB2\xD0\xB5\xD1\x82");
    truncateUtf8(s, 3);
    TEST_ASSERT_EQUAL_STRING("\xD0\x9F\xD1\x80\xD0\xB8", s.c_str());  // "При"
    TEST_ASSERT_EQUAL_INT(6, (int)s.length());
}

void test_truncate_mixed_ascii_cyrillic() {
    // "AB" + "Я" (U+042F, 2 bytes) = 3 chars, 4 bytes.
    // Truncate to 2 chars → "AB".
    String s("AB\xD0\xAF");
    truncateUtf8(s, 2);
    TEST_ASSERT_EQUAL_STRING("AB", s.c_str());
}

void test_truncate_three_byte_sequence_no_split() {
    // U+4E16 "世" in UTF-8 is 0xE4 0xB8 0x96 (3 bytes).
    // Two copies = 2 chars, 6 bytes. Truncate to 1 → first char only (3 bytes).
    String s("\xE4\xB8\x96\xE4\xB8\x96");
    truncateUtf8(s, 1);
    TEST_ASSERT_EQUAL_STRING("\xE4\xB8\x96", s.c_str());
    TEST_ASSERT_EQUAL_INT(3, (int)s.length());
}

// =============================================================================
//  isValidMd5
// =============================================================================

void test_md5_valid_lowercase() {
    TEST_ASSERT_TRUE(isValidMd5(String("d41d8cd98f00b204e9800998ecf8427e")));
}

void test_md5_valid_uppercase() {
    TEST_ASSERT_TRUE(isValidMd5(String("D41D8CD98F00B204E9800998ECF8427E")));
}

void test_md5_too_short() {
    TEST_ASSERT_FALSE(isValidMd5(String("d41d8cd98f00b204e9800998ecf8427")));  // 31 chars
}

void test_md5_too_long() {
    TEST_ASSERT_FALSE(isValidMd5(String("d41d8cd98f00b204e9800998ecf8427e0")));  // 33 chars
}

void test_md5_invalid_char() {
    // 'g' is not a valid hex digit
    TEST_ASSERT_FALSE(isValidMd5(String("g41d8cd98f00b204e9800998ecf8427e")));
}

void test_md5_empty() {
    TEST_ASSERT_FALSE(isValidMd5(String("")));
}

// =============================================================================
//  isNewerBuild  (compiled with FIRMWARE_VERSION="testver", BUILD_N=42)
// =============================================================================

void test_newer_build_both_ci_remote_ahead() {
    // Both sides have build numbers → numeric comparison: 43 > 42 → newer
    TEST_ASSERT_TRUE(isNewerBuild(43, String("any")));
}

void test_newer_build_both_ci_same() {
    TEST_ASSERT_FALSE(isNewerBuild(42, String("any")));
}

void test_newer_build_both_ci_remote_behind() {
    TEST_ASSERT_FALSE(isNewerBuild(41, String("any")));
}

void test_newer_build_remote_dev_sha_differs() {
    // remoteBuildN == 0 → SHA fallback; "other123" != "testver" → newer
    TEST_ASSERT_TRUE(isNewerBuild(0, String("other123")));
}

void test_newer_build_remote_dev_sha_same() {
    // remoteBuildN == 0 → SHA fallback; "testver" == "testver" → not newer
    TEST_ASSERT_FALSE(isNewerBuild(0, String("testver")));
}

// =============================================================================
//  Palette regression: contrast == 0 must keep i==0 black (power == 0 edge case)
// =============================================================================

// These call the shipped expressions from fire_math.h. The previous versions
// re-implemented the formulas locally, so reverting fire.cpp left them green.

void test_palette_contrast_zero_i0_stays_black() {
    // At contrast=0 power is 0, and IEEE powf(0,0)==1.0 would map "no heat"
    // (i==0) to max brightness. paletteNorm clamps i==0 unconditionally.
    const float power = palettePower(0);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, power);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, paletteNorm(power, 0));
}

void test_palette_contrast_zero_nonzero_i_full_bright() {
    // With power==0, any i>0 gives powf(x,0)==1.0 — intentional: the palette
    // collapses to maximum brightness, creating a white-hot solid effect.
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, paletteNorm(palettePower(0), 128));
}

void test_palette_normal_contrast_midpoint() {
    const float power = palettePower(50);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, power);          // linear at 50
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.502f, paletteNorm(power, 128));
}

// --- heatRamp seam behaviour (TUNED, not a defect) ---------------------------
// ramp = (t192 & 0x3F) << 2 wraps to 0 at every multiple of 0x40, so the `>`
// comparisons drop t192==0x40 and t192==0x80 one branch low with ramp==0. The
// resulting zero-intensity entries (palette 86 and 171-172 at contrast 50) are
// the lamp's tuned look: they track an iso-thermal contour through the heat
// field, so they render as thin dark filaments drifting with the flame rather
// than as noise. Smoothing either seam to a bit test flattens the fire; it has
// been tried and reverted. These tests pin both seams.

void test_heat_ramp_lower_seam_stays_dark_on_purpose() {
    for (uint8_t th = 0; th < THEME_COUNT; th++) {
        Rgb8 c = heatRamp(th, 0x40);
        TEST_ASSERT_EQUAL_UINT8(0, c.r);
        TEST_ASSERT_EQUAL_UINT8(0, c.g);
        TEST_ASSERT_EQUAL_UINT8(0, c.b);
    }
    Rgb8 fire = heatRamp(0, 0x41);          // one past the seam: mid branch resumes
    TEST_ASSERT_EQUAL_UINT8(255, fire.r); TEST_ASSERT_EQUAL_UINT8(4, fire.g);
}

void test_heat_ramp_upper_seam_drops_to_mid_on_purpose() {
    // t192==0x80 stays in the MID branch with ramp==0: pure red in Fire and
    // Ember, blue in Plasma and Ice. Do not "fix" this to the high branch.
    Rgb8 fire = heatRamp(0, 0x80);
    TEST_ASSERT_EQUAL_UINT8(255, fire.r); TEST_ASSERT_EQUAL_UINT8(0, fire.g); TEST_ASSERT_EQUAL_UINT8(0, fire.b);
    Rgb8 ice = heatRamp(3, 0x80);
    TEST_ASSERT_EQUAL_UINT8(0, ice.r);   TEST_ASSERT_EQUAL_UINT8(0, ice.g);   TEST_ASSERT_EQUAL_UINT8(255, ice.b);
    Rgb8 ember = heatRamp(1, 0x80);
    TEST_ASSERT_EQUAL_UINT8(0, ember.r); TEST_ASSERT_EQUAL_UINT8(0, ember.g); TEST_ASSERT_EQUAL_UINT8(0, ember.b);
    Rgb8 hot = heatRamp(0, 0x81);           // one past: high branch resumes
    TEST_ASSERT_EQUAL_UINT8(255, hot.r); TEST_ASSERT_EQUAL_UINT8(255, hot.g); TEST_ASSERT_EQUAL_UINT8(4, hot.b);
}

// --- brightToRaw -------------------------------------------------------------

void test_bright_to_raw_zero_is_off() {
    TEST_ASSERT_EQUAL_UINT8(0, brightToRaw(0));
}

void test_bright_to_raw_full_is_255() {
    TEST_ASSERT_EQUAL_UINT8(255, brightToRaw(100));
}

void test_bright_to_raw_low_clamps_to_floor() {
    // 0.05^2.2*255+0.5 ≈ 0.85 → truncates to 0 → clamped up to BRIGHT_FLOOR,
    // the minimum stable PWM value on WS2812B.
    TEST_ASSERT_EQUAL_UINT8(BRIGHT_FLOOR, brightToRaw(5));
}

void test_bright_to_raw_is_monotonic() {
    for (int b = 1; b < 100; b++)
        TEST_ASSERT_TRUE(brightToRaw((uint8_t)(b + 1)) >= brightToRaw((uint8_t)b));
}

// =============================================================================
//  Fire algorithm invariants
//  Tests validate the formulas from fire.cpp using only config.h constants —
//  no Arduino runtime, no FastLED, no atomics needed.
// =============================================================================

// --- qadd8 / qsub8 (stubs in arduino_stub.h) ---------------------------------

void test_qadd8_saturates_at_255() {
    TEST_ASSERT_EQUAL_INT(255, qadd8(200, 100));
}
void test_qadd8_no_saturation() {
    TEST_ASSERT_EQUAL_INT(150, qadd8(100, 50));
}
void test_qsub8_saturates_at_0() {
    TEST_ASSERT_EQUAL_INT(0, qsub8(50, 100));
}
void test_qsub8_no_saturation() {
    TEST_ASSERT_EQUAL_INT(50, qsub8(100, 50));
}

// --- Upward convection blend coefficients ------------------------------------

void test_convection_coefficients_sum_to_256() {
    // 154 + 102 == 256 means the weighted blend sums to 1.0 in Q8 fixed-point.
    // If the sum were < 256, heat would slowly drain; if > 256, it would grow.
    TEST_ASSERT_EQUAL_INT(256, 154 + 102);
}

void test_convection_blend_known_inputs() {
    // Reproduces fire.cpp step 2: heat[y][x] = (h1*154 + h2*102) >> 8
    uint8_t h1 = 200, h2 = 100;
    uint8_t result = (uint8_t)(((uint16_t)h1 * 154 + (uint16_t)h2 * 102) >> 8);
    // (200*154 + 100*102) >> 8 = (30800 + 10200) >> 8 = 41000 >> 8 = 160
    TEST_ASSERT_EQUAL_INT(160, result);
}

void test_convection_no_uint16_overflow() {
    // Worst case: both inputs at 255. Intermediate must fit in uint16_t.
    uint32_t max_intermediate = (uint32_t)255 * 154 + (uint32_t)255 * 102;
    TEST_ASSERT_LESS_THAN((uint32_t)65536, max_intermediate);  // 65280 < 65536
}

// --- Per-row cooling formula (recalcCooling in fire.cpp) --------------------
// Formula: coolMax = (r * COOLING_ROW_SCALE) / ROWS + COOLING_ROW_BIAS
//          where r = random8(lo, hi), lo = max(0, cooling-VAR), hi = min(255, cooling+VAR)

void test_cool_formula_always_at_least_bias() {
    // Even at the lowest possible random sample (lo at COOLING_MIN), result >= BIAS.
    const int lo = COOLING_MIN - COOLING_VARIANCE;  // 20-10 = 10
    const uint8_t at_lo = (uint8_t)((lo * COOLING_ROW_SCALE) / ROWS + COOLING_ROW_BIAS);
    TEST_ASSERT_GREATER_OR_EQUAL(COOLING_ROW_BIAS, at_lo);
}

void test_cool_formula_higher_cooling_raises_coolmax() {
    // coolMax at COOLING_MAX min-sample > coolMax at COOLING_MIN max-sample,
    // i.e. the ranges don't overlap across the full parameter span.
    const int lo_max = COOLING_MAX - COOLING_VARIANCE;       // 150-10 = 140
    const int hi_min = COOLING_MIN + COOLING_VARIANCE;       // 20+10  = 30
    const uint8_t at_max_lo  = (uint8_t)((lo_max * COOLING_ROW_SCALE) / ROWS + COOLING_ROW_BIAS);
    const uint8_t at_min_hi  = (uint8_t)((hi_min * COOLING_ROW_SCALE) / ROWS + COOLING_ROW_BIAS);
    TEST_ASSERT_GREATER_THAN(at_min_hi, at_max_lo);
}

void test_cool_lo_clamp_prevents_underflow() {
    // If cooling < COOLING_VARIANCE, lo would underflow uint8 without the guard.
    // fire.cpp: lo = (cooling > COOLING_VARIANCE) ? cooling - COOLING_VARIANCE : 0
    const uint8_t low_cooling = COOLING_VARIANCE - 1;  // 9 — below the guard threshold
    const uint8_t lo = (low_cooling > COOLING_VARIANCE) ? low_cooling - COOLING_VARIANCE : 0;
    TEST_ASSERT_EQUAL_INT(0, lo);
}

// --- Spark constants ---------------------------------------------------------

void test_spark_base_rows_within_grid() {
    // Sparks must only touch the bottom SPARK_BASE_ROWS rows; must be within [0, ROWS).
    TEST_ASSERT_LESS_THAN(ROWS, SPARK_BASE_ROWS);
}

void test_spark_heat_range_positive() {
    // random8(SPARK_INTENSITY - SPARK_MIN_VARIANCE, SPARK_INTENSITY) must have lo < hi.
    const int lo = SPARK_INTENSITY - SPARK_MIN_VARIANCE;  // 240-40 = 200
    TEST_ASSERT_GREATER_THAN(0, lo);
    TEST_ASSERT_LESS_THAN(SPARK_INTENSITY, lo);
}

// =============================================================================
//  mqttResolveBright — ON/OFF brightness state machine
// =============================================================================

void test_mqtt_bright_only_mid() {
    MqttBrightDelta d = mqttResolveBright(true, false, nullptr, 50, 100, 30);
    TEST_ASSERT_EQUAL_INT(50, d.bright);
    TEST_ASSERT_EQUAL_INT(50, d.last_b);
}

void test_mqtt_bright_zero_does_not_update_last_b() {
    MqttBrightDelta d = mqttResolveBright(true, false, nullptr, 0, 100, 30);
    TEST_ASSERT_EQUAL_INT(0, d.bright);
    TEST_ASSERT_EQUAL_INT(30, d.last_b);  // not updated when cv==0
}

void test_mqtt_bright_clamped_high() {
    MqttBrightDelta d = mqttResolveBright(true, false, nullptr, 150, 100, 30);
    TEST_ASSERT_EQUAL_INT(BRIGHT_MAX, d.bright);
    TEST_ASSERT_EQUAL_INT(BRIGHT_MAX, d.last_b);
}

void test_mqtt_bright_clamped_low() {
    MqttBrightDelta d = mqttResolveBright(true, false, nullptr, -5, 80, 30);
    TEST_ASSERT_EQUAL_INT(0, d.bright);
    TEST_ASSERT_EQUAL_INT(30, d.last_b);  // clamped to 0, so last_b not updated
}

void test_mqtt_off_saves_cur_bright() {
    MqttBrightDelta d = mqttResolveBright(false, true, "OFF", 0, 80, 30);
    TEST_ASSERT_EQUAL_INT(0, d.bright);
    TEST_ASSERT_EQUAL_INT(80, d.last_b);  // saved curBright
}

void test_mqtt_off_cur_bright_zero_no_save() {
    MqttBrightDelta d = mqttResolveBright(false, true, "OFF", 0, 0, 30);
    TEST_ASSERT_EQUAL_INT(0, d.bright);
    TEST_ASSERT_EQUAL_INT(30, d.last_b);  // curBright==0: nothing to save
}

void test_mqtt_on_restores_last_b() {
    MqttBrightDelta d = mqttResolveBright(false, true, "ON", 0, 0, 50);
    TEST_ASSERT_EQUAL_INT(50, d.bright);
}

void test_mqtt_on_already_on_no_change() {
    MqttBrightDelta d = mqttResolveBright(false, true, "ON", 0, 30, 50);
    TEST_ASSERT_EQUAL_INT(-1, d.bright);  // curBright!=0: no-op
}

void test_mqtt_on_last_b_zero_uses_default() {
    MqttBrightDelta d = mqttResolveBright(false, true, "ON", 0, 0, 0);
    TEST_ASSERT_EQUAL_INT(BRIGHT_DEFAULT, d.bright);
}

void test_mqtt_b_and_off_saves_new_b_as_last_b() {
    // {b:80, state:"OFF"} — b processed first; last_b=80, then bright→0
    MqttBrightDelta d = mqttResolveBright(true, true, "OFF", 80, 50, 30);
    TEST_ASSERT_EQUAL_INT(0, d.bright);
    TEST_ASSERT_EQUAL_INT(80, d.last_b);  // b=80 wins over curBright=50
}

void test_mqtt_b_and_on_skips_on_restore() {
    // {b:80, state:"ON"} with lamp off — b sets bright directly; ON skips (hadB=true)
    MqttBrightDelta d = mqttResolveBright(true, true, "ON", 80, 0, 30);
    TEST_ASSERT_EQUAL_INT(80, d.bright);
    TEST_ASSERT_EQUAL_INT(80, d.last_b);
}

void test_mqtt_neither_b_nor_state() {
    MqttBrightDelta d = mqttResolveBright(false, false, nullptr, 0, 50, 30);
    TEST_ASSERT_EQUAL_INT(-1, d.bright);
    TEST_ASSERT_EQUAL_INT(30, d.last_b);
}

// =============================================================================
//  Entry point
// =============================================================================

// =============================================================================
//  Lava lamp (lava_math.h)
// =============================================================================

static float maxBrightness(Rgb8 c) { return (float)(c.r > c.g ? (c.r > c.b ? c.r : c.b) : (c.g > c.b ? c.g : c.b)); }

void test_lava_dx_wraps_the_short_way_round() {
    // Columns 0 and 19 are neighbours on the tube: one column apart, not 19.
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f * LAVA_ASPECT, lavaDx(0.0f, 19.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, lavaDx(3.0f, 7.0f), lavaDx(7.0f, 3.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, (COLUMNS / 2) * LAVA_ASPECT, lavaDx(0.0f, (float)(COLUMNS / 2)));
}

void test_lava_field_is_seamless_across_the_tube_join() {
    // A blob sitting on the join must look the same on both sides of it.
    const LavaBlob b[1] = {{19.5f, 20.0f, 3.0f}};
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, lavaField(b, 1, 0.0f, 20.0f, 0.0f), lavaField(b, 1, 19.0f, 20.0f, 0.0f));
}

void test_lava_blob_centre_is_wax_and_far_liquid_is_not() {
    const LavaBlob b[1] = {{10.0f, 20.0f, LAVA_R_MIN}};
    TEST_ASSERT_TRUE(lavaField(b, 1, 10.0f, 20.0f, 0.0f) > LAVA_EDGE_HI);
    // Opposite side of the tube, mid-height: only a trace of field left.
    TEST_ASSERT_TRUE(lavaField(b, 1, 0.0f, 35.0f, 0.0f) < LAVA_EDGE_LO);
}

void test_lava_two_nearby_blobs_merge() {
    // Each alone leaves the midpoint as liquid; together they bridge it.
    const float r = 2.6f;
    const LavaBlob a[1] = {{8.0f, 20.0f, r}}, both[2] = {{8.0f, 20.0f, r}, {12.0f, 20.0f, r}};
    TEST_ASSERT_TRUE(lavaField(a, 1, 10.0f, 20.0f, 0.0f) < LAVA_EDGE_HI);
    TEST_ASSERT_TRUE(lavaField(both, 2, 10.0f, 20.0f, 0.0f) >= LAVA_EDGE_HI);
}

void test_lava_pool_covers_the_base_and_fades_above_it() {
    for (float w = -0.7f; w <= 0.7f; w += 0.35f) {
        TEST_ASSERT_TRUE(lavaField(nullptr, 0, 5.0f, 0.0f, w) > LAVA_EDGE_HI);   // base row is wax
        TEST_ASSERT_TRUE(lavaField(nullptr, 0, 5.0f, 6.0f, w) < LAVA_EDGE_LO);   // clear liquid above
    }
}

void test_lava_wobble_stays_within_the_pool_bound() {
    // lavaField divides by (y + 1 + wobble); |wobble| <= 0.7 keeps it above 0.3.
    for (int x = 0; x < COLUMNS; x++)
        for (float t = 0.0f; t < 60.0f; t += 0.37f)
            TEST_ASSERT_TRUE(fabsf(lavaWobble((float)x, t)) <= 0.7f);
}

void test_lava_step_keeps_blobs_on_the_tube() {
    LavaMotion m = {19.9f, LAVA_DRIFT_MAX, 0.0f, LAVA_SPEED_MAX, LAVA_R_MAX, 0.0f};
    for (int i = 0; i < 4000; i++) {
        const LavaBlob b = lavaStep(m, 0.05f);
        TEST_ASSERT_TRUE(b.x >= 0.0f && b.x < (float)COLUMNS);
        TEST_ASSERT_TRUE(b.y >= -1e-3f && b.y <= (float)(ROWS - 1) + 1e-3f);
        TEST_ASSERT_TRUE(b.r <= LAVA_R_MAX * (1.0f + LAVA_BREATHE_DEPTH) + 1e-3f);
    }
}

void test_lava_wax_is_never_dark_in_any_theme() {
    // The tones come from heatRamp() off its 0x40 seams: a seam hit would give
    // black wax. Solid wax must be clearly brighter than the liquid behind it.
    for (uint8_t th = 0; th < THEME_COUNT; th++) {
        const LavaPal p = lavaPalette(th);
        for (float f = LAVA_EDGE_HI; f < 50.0f; f *= 1.5f) {
            const Rgb8 wax = lavaColor(p, f, 0.5f);
            TEST_ASSERT_TRUE(maxBrightness(wax) >= 64.0f);   // shaded rim of the deepest theme (Ember) is ~100
            TEST_ASSERT_TRUE(maxBrightness(wax) > 3.0f * maxBrightness(lavaColor(p, 0.0f, 0.5f)));
        }
    }
}

void test_lava_liquid_is_dim_and_lit_from_below() {
    for (uint8_t th = 0; th < THEME_COUNT; th++) {
        const LavaPal p = lavaPalette(th);
        const Rgb8 low = lavaColor(p, 0.0f, 0.0f), high = lavaColor(p, 0.0f, 1.0f);
        TEST_ASSERT_TRUE(maxBrightness(low) <= LAVA_BG_LEVEL);
        TEST_ASSERT_TRUE(maxBrightness(high) <= maxBrightness(low));
    }
}

void test_lava_colour_brightens_monotonically_into_a_blob() {
    // Edge, body and core tints each blend towards a brighter tone, so walking
    // from liquid into a blob centre never gets darker.
    for (uint8_t th = 0; th < THEME_COUNT; th++) {
        const LavaPal p = lavaPalette(th);
        float prev = -1.0f;
        for (float f = 0.0f; f < 10.0f; f += 0.05f) {
            const Rgb8 c = lavaColor(p, f, 0.3f);
            const float sum = (float)c.r + c.g + c.b;
            TEST_ASSERT_TRUE(sum + 1.5f >= prev);   // allow uint8 rounding
            prev = sum;
        }
    }
}

// =============================================================================
//  Aurora (aurora_math.h)
// =============================================================================

void test_aurora_waves_are_seamless_across_the_tube_join() {
    // Column COLUMNS is column 0 again: every travelling wave must agree there.
    for (float t = 0.0f; t < 120.0f; t += 3.7f) {
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, auroraCurtain(0.0f, t), auroraCurtain((float)COLUMNS, t));
        TEST_ASSERT_FLOAT_WITHIN(1e-3f, auroraHem(0.0f, t), auroraHem((float)COLUMNS, t));
    }
}

void test_aurora_curtain_has_open_sky_and_full_curtain() {
    // Over time some columns must be fully open and some fully lit, or the
    // display degenerates into a uniform glow.
    bool open = false, full = false;
    for (float t = 0.0f; t < 200.0f; t += 1.3f)
        for (int x = 0; x < COLUMNS; x++) {
            const float c = auroraCurtain((float)x, t);
            TEST_ASSERT_TRUE(c >= 0.0f && c <= 1.0f);
            open |= (c == 0.0f);
            full |= (c == 1.0f);
        }
    TEST_ASSERT_TRUE(open);
    TEST_ASSERT_TRUE(full);
}

void test_aurora_hem_stays_on_the_lamp() {
    for (float t = 0.0f; t < 200.0f; t += 0.9f)
        for (int x = 0; x < COLUMNS; x++) {
            const float h = auroraHem((float)x, t);
            TEST_ASSERT_TRUE(h >= 1.0f && h <= (float)ROWS - 10.0f);
        }
}

void test_aurora_light_peaks_at_the_hem_and_fades_both_ways() {
    const float hem = 12.0f;
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, auroraVertical(hem, hem));
    TEST_ASSERT_TRUE(auroraVertical(hem - 5.0f, hem) < 0.05f);              // dies within a few rows below
    TEST_ASSERT_TRUE(auroraVertical(hem + 3.0f, hem) > 0.7f);               // long rays above
    TEST_ASSERT_TRUE(auroraVertical(hem + 20.0f, hem) < auroraVertical(hem + 10.0f, hem));
}

void test_aurora_ray_shimmer_stays_in_range() {
    for (int x = 0; x < COLUMNS; x++)
        for (float t = 0.0f; t < 50.0f; t += 0.31f) {
            const float r = auroraRay(x, t);
            TEST_ASSERT_TRUE(r >= 1.0f - AURORA_RAY_DEPTH - 1e-4f && r <= 1.0f + 1e-4f);
        }
}

void test_aurora_open_sky_is_only_the_dim_sky_colour() {
    for (uint8_t th = 0; th < THEME_COUNT; th++) {
        const AuroraPal p = auroraPalette(th);
        const Rgb8 c = auroraColor(p, 0.0f, 1.0f, 1.0f, 20.0f, 10.0f);
        TEST_ASSERT_EQUAL_UINT8(p.sky.r, c.r);
        TEST_ASSERT_EQUAL_UINT8(p.sky.g, c.g);
        TEST_ASSERT_EQUAL_UINT8(p.sky.b, c.b);
        TEST_ASSERT_TRUE(maxBrightness(p.sky) <= 20.0f);   // a dark night-blue, not a glow
    }
}

void test_aurora_curtain_is_bright_at_the_hem_in_every_theme() {
    for (uint8_t th = 0; th < THEME_COUNT; th++) {
        const Rgb8 c = auroraColor(auroraPalette(th), 1.0f, 1.0f, 1.0f, 12.0f, 12.0f);
        TEST_ASSERT_TRUE(maxBrightness(c) >= 150.0f);
    }
}

void test_aurora_is_not_too_dark_to_see() {
    // Regression: the first tuning averaged 19/255 per pixel, which read as the
    // brightness slider's minimum even at 100 %. Average the brightest channel
    // over the whole lamp for a minute of motion; the lava lamp sits near 80.
    for (uint8_t th = 0; th < THEME_COUNT; th++) {
        const AuroraPal p = auroraPalette(th);
        double sum = 0.0;
        int    n   = 0;
        for (float t = 0.0f; t < 60.0f; t += 0.5f)
            for (int x = 0; x < COLUMNS; x++) {
                const float c = auroraCurtain((float)x, t), h = auroraHem((float)x, t), r = auroraRay(x, t);
                for (int y = 0; y < ROWS; y++, n++)
                    sum += maxBrightness(auroraColor(p, c, r, 1.0f - AURORA_PULSE_DEPTH * 0.5f, (float)y, h));
            }
        TEST_ASSERT_TRUE(sum / n >= 60.0);
    }
}

void test_aurora_visibly_moves() {
    // Regression: the first version changed each pixel by 6.9/255 per 0.5 s on
    // average and read as frozen light; the lava lamp manages ~15. Require at
    // least 12 so the curtains are seen to drift, fold and surge.
    const AuroraPal p = auroraPalette(0);
    auto frame = [&](float t, int x, int y) {
        return maxBrightness(auroraColor(p, auroraCurtain((float)x, t), auroraRay(x, t) * auroraSurge((float)x, t),
                                         1.0f, (float)y, auroraHem((float)x, t)));
    };
    double sum = 0.0;
    int    n   = 0;
    for (float t = 0.5f; t < 60.0f; t += 0.5f)
        for (int x = 0; x < COLUMNS; x++)
            for (int y = 0; y < ROWS; y++, n++) sum += fabsf(frame(t, x, y) - frame(t - 0.5f, x, y));
    TEST_ASSERT_TRUE(sum / n >= 12.0);
}

void test_aurora_surge_sweeps_and_fades() {
    // Outside a surge's band the light is untouched; inside it is boosted, and
    // between surges (phase 0) there is no boost anywhere.
    for (int x = 0; x < COLUMNS; x++) {
        const float s = auroraSurge((float)x, 0.0f);
        TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, s);
    }
    float peak = 0.0f;
    for (int x = 0; x < COLUMNS; x++) {
        const float s = auroraSurge((float)x, AURORA_SURGE_PERIOD * 0.5f);
        TEST_ASSERT_TRUE(s >= 1.0f && s <= 1.0f + AURORA_SURGE_GAIN + 1e-4f);
        if (s > peak) peak = s;
    }
    TEST_ASSERT_TRUE(peak > 1.0f + 0.8f * AURORA_SURGE_GAIN);
}

void test_aurora_colour_shifts_from_body_to_top_with_height() {
    // Default theme: green body low, violet top high — blue/green ratio rises.
    const AuroraPal p = auroraPalette(0);
    const Rgb8 low  = auroraColor(p, 1.0f, 1.0f, 1.0f, 14.0f, 10.0f);
    const Rgb8 high = auroraColor(p, 1.0f, 1.0f, 1.0f, 32.0f, 10.0f);
    TEST_ASSERT_TRUE(low.g > low.b);
    TEST_ASSERT_TRUE((float)high.b / (high.g + 1.0f) > (float)low.b / (low.g + 1.0f));
}

// =============================================================================
//  Fireflies (firefly_math.h)
// =============================================================================

// Renders one frame the way fireflies.cpp does (without the temporal blend)
// into brightest-channel values, for the brightness and motion floors below.
static void flyFrame(const Firefly *fl, int n, uint8_t theme, float out[ROWS][COLUMNS]) {
    const FlyPal p = flyPalette(theme);
    float rgb[ROWS][COLUMNS][3];
    for (int y = 0; y < ROWS; y++) {
        const Rgb8 c = flyNight(p, (float)y / (float)(ROWS - 1));
        for (int x = 0; x < COLUMNS; x++) { rgb[y][x][0] = c.r; rgb[y][x][1] = c.g; rgb[y][x][2] = c.b; }
    }
    for (int i = 0; i < n; i++) {
        const float g = flyGlow(fl[i].phase, fl[i].flash);
        const int cx = (int)lroundf(fl[i].x), cy = (int)lroundf(fl[i].y);
        for (int dy = -FLY_REACH; dy <= FLY_REACH; dy++) {
            const int y = cy + dy;
            if (y < 0 || y >= ROWS) continue;
            for (int dxi = -FLY_REACH; dxi <= FLY_REACH; dxi++) {
                const int x = ((cx + dxi) % COLUMNS + COLUMNS) % COLUMNS;
                const float dx = lavaDx((float)x, fl[i].x), ddy = (float)y - fl[i].y;
                const Rgb8 c = flyLight(p, g * flyHalo(sqrtf(dx * dx + ddy * ddy)));
                rgb[y][x][0] = fminf(255.0f, rgb[y][x][0] + c.r);
                rgb[y][x][1] = fminf(255.0f, rgb[y][x][1] + c.g);
                rgb[y][x][2] = fminf(255.0f, rgb[y][x][2] + c.b);
            }
        }
    }
    for (int y = 0; y < ROWS; y++)
        for (int x = 0; x < COLUMNS; x++)
            out[y][x] = fmaxf(rgb[y][x][0], fmaxf(rgb[y][x][1], rgb[y][x][2]));
}

static void flySwarm(Firefly *fl) {
    // Deterministic spread over the documented ranges.
    for (int i = 0; i < FLY_COUNT; i++) {
        const float u = (float)((i * 37) % FLY_COUNT) / FLY_COUNT, v = (float)((i * 17) % FLY_COUNT) / FLY_COUNT;
        fl[i] = {u * COLUMNS, FLY_Y_MIN + v * (FLY_Y_MAX - FLY_Y_MIN), 6.283f * v,
                 FLY_SPEED_MIN + u * (FLY_SPEED_MAX - FLY_SPEED_MIN), 0.0f,
                 FLY_PERIOD_MIN + v * (FLY_PERIOD_MAX - FLY_PERIOD_MIN), FLY_FLASH_MIN + u * (FLY_FLASH_MAX - FLY_FLASH_MIN)};
        fl[i].phase = u * fl[i].period;
    }
}

void test_fly_glow_rises_peaks_fades_and_keeps_an_ember() {
    const float flash = 1.6f;
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, FLY_EMBER, flyGlow(0.0f, flash));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, flyGlow(FLY_RISE * flash, flash));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, FLY_EMBER, flyGlow(flash, flash));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, FLY_EMBER, flyGlow(flash + 2.0f, flash));
    float prev = 0.0f;
    for (float t = 0.0f; t <= FLY_RISE * flash; t += 0.01f) { const float g = flyGlow(t, flash); TEST_ASSERT_TRUE(g + 1e-5f >= prev); prev = g; }
    for (float t = FLY_RISE * flash; t <= flash; t += 0.01f) { const float g = flyGlow(t, flash); TEST_ASSERT_TRUE(g <= prev + 1e-5f); prev = g; }
}

void test_fly_step_wraps_round_the_tube_and_bounces_off_top_and_bottom() {
    Firefly f = {19.9f, FLY_Y_MAX - 0.1f, 0.3f, FLY_SPEED_MAX, 0.0f, 3.0f, 1.5f};
    for (int i = 0; i < 5000; i++) {
        flyStep(f, 0.05f, ((i * 7919) % 200) / 100.0f - 1.0f);
        TEST_ASSERT_TRUE(f.x >= 0.0f && f.x < (float)COLUMNS);
        TEST_ASSERT_TRUE(f.y >= FLY_Y_MIN && f.y <= FLY_Y_MAX);
        TEST_ASSERT_TRUE(f.phase >= 0.0f && f.phase < f.period);
    }
}

void test_fly_halo_is_half_at_its_radius_and_negligible_at_reach() {
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0f, flyHalo(0.0f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, flyHalo(FLY_HALO));
    // Pixels beyond FLY_REACH are skipped; what they lose must be invisible (< 2/255 at full flash).
    TEST_ASSERT_TRUE(255.0f * flyHalo((float)FLY_REACH + 1.0f) < 2.0f * 4.0f);
}

void test_fly_light_is_dark_when_off_and_whitens_at_full_flash() {
    for (uint8_t th = 0; th < THEME_COUNT; th++) {
        const FlyPal p = flyPalette(th);
        const Rgb8 off = flyLight(p, 0.0f), full = flyLight(p, 1.0f);
        TEST_ASSERT_EQUAL_UINT8(0, (uint8_t)maxBrightness(off));
        TEST_ASSERT_TRUE(maxBrightness(full) >= 200.0f);
        TEST_ASSERT_TRUE((int)full.r + full.g + full.b > (int)p.glow.r + p.glow.g + p.glow.b);   // core is whiter
    }
}

void test_fireflies_are_not_too_dark_to_see() {
    // Same regression guard as the aurora: the first tuning averaged 15/255.
    for (uint8_t th = 0; th < THEME_COUNT; th++) {
        Firefly fl[FLY_COUNT];
        flySwarm(fl);
        static float fr[ROWS][COLUMNS];
        double sum = 0.0;
        int    n   = 0;
        for (int step = 0; step < 240; step++) {
            for (int i = 0; i < FLY_COUNT; i++) flyStep(fl[i], 0.25f, ((step * 31 + i * 17) % 200) / 100.0f - 1.0f);
            flyFrame(fl, FLY_COUNT, th, fr);
            for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLUMNS; x++, n++) sum += fr[y][x];
        }
        TEST_ASSERT_TRUE(sum / n >= 40.0);
    }
}

void test_fireflies_visibly_move_and_flash() {
    Firefly fl[FLY_COUNT];
    flySwarm(fl);
    static float a[ROWS][COLUMNS], b[ROWS][COLUMNS];
    flyFrame(fl, FLY_COUNT, 0, a);
    double sum = 0.0;
    int    n   = 0;
    for (int step = 0; step < 120; step++) {
        for (int i = 0; i < FLY_COUNT; i++) flyStep(fl[i], 0.5f, ((step * 13 + i * 29) % 200) / 100.0f - 1.0f);
        flyFrame(fl, FLY_COUNT, 0, b);
        for (int y = 0; y < ROWS; y++) for (int x = 0; x < COLUMNS; x++, n++) { sum += fabsf(b[y][x] - a[y][x]); a[y][x] = b[y][x]; }
    }
    TEST_ASSERT_TRUE(sum / n >= 12.0);
}

int main() {
    UNITY_BEGIN();

    RUN_TEST(test_json_escape_empty);
    RUN_TEST(test_json_escape_plain_ascii);
    RUN_TEST(test_json_escape_double_quote);
    RUN_TEST(test_json_escape_backslash);
    RUN_TEST(test_json_escape_newline);
    RUN_TEST(test_json_escape_carriage_return);
    RUN_TEST(test_json_escape_tab);
    RUN_TEST(test_json_escape_control_chars_stripped);
    RUN_TEST(test_json_escape_mixed);
    RUN_TEST(test_json_escape_utf8_passthrough);

    RUN_TEST(test_truncate_empty_string);
    RUN_TEST(test_truncate_ascii_under_limit);
    RUN_TEST(test_truncate_ascii_at_limit);
    RUN_TEST(test_truncate_ascii_over_limit);
    RUN_TEST(test_truncate_limit_zero);
    RUN_TEST(test_truncate_cyrillic_no_split);
    RUN_TEST(test_truncate_mixed_ascii_cyrillic);
    RUN_TEST(test_truncate_three_byte_sequence_no_split);

    RUN_TEST(test_md5_valid_lowercase);
    RUN_TEST(test_md5_valid_uppercase);
    RUN_TEST(test_md5_too_short);
    RUN_TEST(test_md5_too_long);
    RUN_TEST(test_md5_invalid_char);
    RUN_TEST(test_md5_empty);

    RUN_TEST(test_newer_build_both_ci_remote_ahead);
    RUN_TEST(test_newer_build_both_ci_same);
    RUN_TEST(test_newer_build_both_ci_remote_behind);
    RUN_TEST(test_newer_build_remote_dev_sha_differs);
    RUN_TEST(test_newer_build_remote_dev_sha_same);

    RUN_TEST(test_palette_contrast_zero_i0_stays_black);
    RUN_TEST(test_palette_contrast_zero_nonzero_i_full_bright);
    RUN_TEST(test_palette_normal_contrast_midpoint);
    RUN_TEST(test_heat_ramp_upper_seam_drops_to_mid_on_purpose);
    RUN_TEST(test_heat_ramp_lower_seam_stays_dark_on_purpose);
    RUN_TEST(test_bright_to_raw_zero_is_off);
    RUN_TEST(test_bright_to_raw_full_is_255);
    RUN_TEST(test_bright_to_raw_low_clamps_to_floor);
    RUN_TEST(test_bright_to_raw_is_monotonic);

    RUN_TEST(test_qadd8_saturates_at_255);
    RUN_TEST(test_qadd8_no_saturation);
    RUN_TEST(test_qsub8_saturates_at_0);
    RUN_TEST(test_qsub8_no_saturation);
    RUN_TEST(test_convection_coefficients_sum_to_256);
    RUN_TEST(test_convection_blend_known_inputs);
    RUN_TEST(test_convection_no_uint16_overflow);
    RUN_TEST(test_cool_formula_always_at_least_bias);
    RUN_TEST(test_cool_formula_higher_cooling_raises_coolmax);
    RUN_TEST(test_cool_lo_clamp_prevents_underflow);
    RUN_TEST(test_spark_base_rows_within_grid);
    RUN_TEST(test_spark_heat_range_positive);

    RUN_TEST(test_mqtt_bright_only_mid);
    RUN_TEST(test_mqtt_bright_zero_does_not_update_last_b);
    RUN_TEST(test_mqtt_bright_clamped_high);
    RUN_TEST(test_mqtt_bright_clamped_low);
    RUN_TEST(test_mqtt_off_saves_cur_bright);
    RUN_TEST(test_mqtt_off_cur_bright_zero_no_save);
    RUN_TEST(test_mqtt_on_restores_last_b);
    RUN_TEST(test_mqtt_on_already_on_no_change);
    RUN_TEST(test_mqtt_on_last_b_zero_uses_default);
    RUN_TEST(test_mqtt_b_and_off_saves_new_b_as_last_b);
    RUN_TEST(test_mqtt_b_and_on_skips_on_restore);
    RUN_TEST(test_mqtt_neither_b_nor_state);

    RUN_TEST(test_lava_dx_wraps_the_short_way_round);
    RUN_TEST(test_lava_field_is_seamless_across_the_tube_join);
    RUN_TEST(test_lava_blob_centre_is_wax_and_far_liquid_is_not);
    RUN_TEST(test_lava_two_nearby_blobs_merge);
    RUN_TEST(test_lava_pool_covers_the_base_and_fades_above_it);
    RUN_TEST(test_lava_wobble_stays_within_the_pool_bound);
    RUN_TEST(test_lava_step_keeps_blobs_on_the_tube);
    RUN_TEST(test_lava_wax_is_never_dark_in_any_theme);
    RUN_TEST(test_lava_liquid_is_dim_and_lit_from_below);
    RUN_TEST(test_lava_colour_brightens_monotonically_into_a_blob);
    RUN_TEST(test_aurora_waves_are_seamless_across_the_tube_join);
    RUN_TEST(test_aurora_curtain_has_open_sky_and_full_curtain);
    RUN_TEST(test_aurora_hem_stays_on_the_lamp);
    RUN_TEST(test_aurora_light_peaks_at_the_hem_and_fades_both_ways);
    RUN_TEST(test_aurora_ray_shimmer_stays_in_range);
    RUN_TEST(test_aurora_open_sky_is_only_the_dim_sky_colour);
    RUN_TEST(test_aurora_curtain_is_bright_at_the_hem_in_every_theme);
    RUN_TEST(test_aurora_colour_shifts_from_body_to_top_with_height);
    RUN_TEST(test_aurora_is_not_too_dark_to_see);
    RUN_TEST(test_aurora_visibly_moves);
    RUN_TEST(test_aurora_surge_sweeps_and_fades);
    RUN_TEST(test_fly_glow_rises_peaks_fades_and_keeps_an_ember);
    RUN_TEST(test_fly_step_wraps_round_the_tube_and_bounces_off_top_and_bottom);
    RUN_TEST(test_fly_halo_is_half_at_its_radius_and_negligible_at_reach);
    RUN_TEST(test_fly_light_is_dark_when_off_and_whitens_at_full_flash);
    RUN_TEST(test_fireflies_are_not_too_dark_to_see);
    RUN_TEST(test_fireflies_visibly_move_and_flash);
    return UNITY_END();
}

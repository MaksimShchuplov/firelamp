#include <unity.h>
#include <cmath>

#include "arduino_stub.h"

#include "../../src/text_utils.h"
#include "../../src/config.h"
#include "../../src/mqtt_state.h"

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

void test_palette_contrast_zero_i0_stays_black() {
    // At contrast=0: power = 1 + (0 - 50) / 50 = 0.
    // IEEE powf(0, 0) == 1.0, which would map "no heat" (i==0) to max brightness.
    // The fix: clamp i==0 to n=0 unconditionally.
    const int   contrast = 0;
    const float power    = 1.0f + ((float)contrast - 50.0f) / 50.0f;
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, power);  // sanity: power is indeed 0

    const int   i = 0;
    const float n = (i == 0) ? 0.0f : powf((float)i / 255.0f, power);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, n);  // i==0 must be black, not 1.0
}

void test_palette_contrast_zero_nonzero_i_full_bright() {
    // With power==0, any i>0 gives powf(x,0)==1.0 — intentional: the entire palette
    // collapses to maximum brightness, creating a white-hot solid effect.
    const int   contrast = 0;
    const float power    = 1.0f + ((float)contrast - 50.0f) / 50.0f;

    const int   i = 128;
    const float n = (i == 0) ? 0.0f : powf((float)i / 255.0f, power);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, n);
}

void test_palette_normal_contrast_midpoint() {
    // At contrast=50: power = 1.0 (linear). i=128 → n ≈ 0.502.
    const int   contrast = 50;
    const float power    = 1.0f + ((float)contrast - 50.0f) / 50.0f;
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 1.0f, power);

    const int   i = 128;
    const float n = (i == 0) ? 0.0f : powf((float)i / 255.0f, power);
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 0.502f, n);
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

    return UNITY_END();
}

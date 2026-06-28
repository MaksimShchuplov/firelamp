#pragma once
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <ArduinoJson.h>
#include "globals.h"

// =============================================================================
//  PARAMETER REGISTRY — single source of truth for every UI parameter
// =============================================================================
// Each parameter used to be described by its range in config.h, clamped
// independently in handlers.cpp / mqtt.cpp / gemini.cpp / presets.cpp, and
// listed across five parallel arrays in presets.cpp. The registry collapses all
// of that into one table: NVS/JSON/MQTT key, live atomic, default, range, and
// the side-effect to run when the value changes.
//
// autoJson = false marks parameters whose JSON handling is NOT a plain clamp
// (brightness goes through setBright()/gamma and the MQTT ON/OFF state machine),
// so applyJsonParams() skips them and the callers apply them explicitly.

struct ParamDesc {
    const char           *key;       // NVS key suffix, JSON field, MQTT field
    std::atomic<uint8_t> *value;     // live cross-core value
    uint8_t               def;       // factory default
    uint8_t               lo, hi;    // inclusive clamp range
    void                (*onChange)();  // buildHeatPalette / recalcCooling / nullptr
    bool                  autoJson;  // false = caller applies it (brightness)
};

extern const ParamDesc PARAMS[];
extern const size_t     PARAM_COUNT;

// Applies every present, numeric, autoJson parameter from a parsed JSON document,
// clamping to range and firing each parameter's side-effect. Returns true if any
// parameter changed. Brightness (autoJson=false) is left to the caller.
//
// Defined inline so the symbol never crosses a translation-unit boundary:
// ArduinoJson's JsonDocument lives in a config-encoded versioned namespace, and a
// cross-TU definition can mangle to a different name than the caller expects
// (an ODR/link mismatch). Inlining sidesteps that entirely — PARAMS/PARAM_COUNT
// stay extern (plain data, namespace-independent).
inline bool applyJsonParams(JsonDocument &doc) {
    bool changed = false;
    for (size_t i = 0; i < PARAM_COUNT; i++) {
        const ParamDesc &p = PARAMS[i];
        if (!p.autoJson || !doc[p.key].is<int>()) continue;
        int v = doc[p.key].as<int>();
        if      (v < p.lo) v = p.lo;
        else if (v > p.hi) v = p.hi;
        p.value->store((uint8_t)v, std::memory_order_relaxed);
        if (p.onChange) p.onChange();
        changed = true;
    }
    return changed;
}

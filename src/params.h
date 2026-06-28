#pragma once
#include <atomic>
#include <cstddef>
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
bool applyJsonParams(JsonDocument &doc);

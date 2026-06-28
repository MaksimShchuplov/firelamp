#include <Arduino.h>
#include "params.h"

// Order is significant only for presets (the JSON key list); brightness is first
// so the preset arrays keep their historical "b,c,co,sp,bl,th" ordering.
const ParamDesc PARAMS[] = {
    {"b",  &uiBright,   BRIGHT_DEFAULT,   BRIGHT_MIN,   BRIGHT_MAX,    nullptr,          false},
    {"c",  &uiContrast, CONTRAST_DEFAULT, CONTRAST_MIN, CONTRAST_MAX,  buildHeatPalette, true},
    {"co", &uiCooling,  COOLING_DEFAULT,  COOLING_MIN,  COOLING_MAX,   recalcCooling,    true},
    {"sp", &uiSparking, SPARKING_DEFAULT, SPARKING_MIN, SPARKING_MAX,  nullptr,          true},
    {"bl", &uiBlend,    BLEND_DEFAULT,    BLEND_MIN,    BLEND_MAX,     nullptr,          true},
    {"th", &uiTheme,    THEME_DEFAULT,    0,            THEME_COUNT-1, buildHeatPalette, true},
};
const size_t PARAM_COUNT = sizeof(PARAMS) / sizeof(PARAMS[0]);

bool applyJsonParams(JsonDocument &doc) {
    bool changed = false;
    for (size_t i = 0; i < PARAM_COUNT; i++) {
        const ParamDesc &p = PARAMS[i];
        if (!p.autoJson || !doc[p.key].is<int>()) continue;
        p.value->store((uint8_t)constrain(doc[p.key].as<int>(), p.lo, p.hi),
                       std::memory_order_relaxed);
        if (p.onChange) p.onChange();
        changed = true;
    }
    return changed;
}

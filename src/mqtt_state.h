#pragma once
#include <cstdint>
#include <cstring>
#include "config.h"

struct MqttBrightDelta {
    int     bright;  // new brightness to apply; -1 = no change
    uint8_t last_b;  // updated restore point
};

// Pure function — resolves new brightness from an MQTT payload with optional
// "b" and/or "state" fields. Processes b before state so that a combined
// {b:N, state:"OFF"} payload saves N as the restore point (not curBright).
// Does not call setBright() or touch atomics — call site handles side effects.
inline MqttBrightDelta mqttResolveBright(
    bool hadB, bool hadState, const char *stateStr,
    int newB, uint8_t curBright, uint8_t lastB)
{
    MqttBrightDelta d{-1, lastB};

    if (hadB) {
        int bv = newB < BRIGHT_MIN ? BRIGHT_MIN : newB > BRIGHT_MAX ? BRIGHT_MAX : newB;
        d.bright = bv;
        if (bv > 0) d.last_b = (uint8_t)bv;
    }

    if (hadState) {
        if (stateStr && strcmp(stateStr, "OFF") == 0) {
            if (!hadB && curBright > 0) d.last_b = curBright;
            d.bright = 0;
        } else if (stateStr && strcmp(stateStr, "ON") == 0 && !hadB && curBright == 0) {
            d.bright = (lastB > 0 ? (int)lastB : BRIGHT_DEFAULT);
        }
    }

    return d;
}

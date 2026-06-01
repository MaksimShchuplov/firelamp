#include <Arduino.h>
#include <Preferences.h>
#include <Update.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include "globals.h"

// Counts consecutive hard crashes (panic / watchdog). On the third crash
// rolls back to the previous OTA firmware partition. Normal power-ons and
// OTA-triggered restarts are ignored.
void safeBootCheck() {
    esp_reset_reason_t r = esp_reset_reason();
    if (r != ESP_RST_PANIC && r != ESP_RST_INT_WDT &&
        r != ESP_RST_TASK_WDT && r != ESP_RST_WDT) return;

    Preferences p;
    p.begin("boot", false);
    uint32_t crashes = p.getUInt("crashes", 0) + 1;
    if (crashes >= 3) {
        LOG_ERROR("boot loop detected — rolling back firmware");
        p.end();
        // Do NOT reset the counter here. If rollback or ESP.restart() never happen
        // (flash error, no slot), the counter stays >= 3 so the next power-on crash
        // also halts immediately rather than silently looping.
        // The rolled-back firmware (or a manually re-flashed image) will call
        // markBootSuccess() on a clean boot, which resets the counter to 0.
        if (Update.canRollBack() && Update.rollBack()) {
            ESP.restart();
        }
        LOG_ERROR("rollback failed or unavailable — halting. Reflash via USB.");
        esp_deep_sleep_start();
    } else {
        LOG_WARN("crash %lu/3 — incrementing counter", (unsigned long)crashes);
        p.putUInt("crashes", crashes);
        p.end();
    }
}

// Call once setup() completes successfully — signals the firmware is stable
// regardless of WiFi state, and cancels any pending OTA auto-rollback.
void markBootSuccess() {
    Preferences p;
    p.begin("boot", false);
    p.putUInt("crashes", 0);
    p.end();
    esp_ota_mark_app_valid_cancel_rollback();
    LOG_INFO("firmware marked as valid (OTA rollback cancelled)");
}

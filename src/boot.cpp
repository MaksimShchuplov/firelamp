#include <Arduino.h>
#include <Preferences.h>
#include <Update.h>
#include <esp_system.h>
#include <esp_ota_ops.h>

// Counts consecutive hard crashes (panic / watchdog). On the third crash
// rolls back to the previous OTA firmware partition. Normal power-ons and
// OTA-triggered restarts are ignored.
void safeBootCheck() {
    esp_reset_reason_t r = esp_reset_reason();
    if (r != ESP_RST_PANIC && r != ESP_RST_INT_WDT &&
        r != ESP_RST_TASK_WDT && r != ESP_RST_WDT) return;

    Preferences p;
    p.begin("lamp", false);
    uint32_t crashes = p.getUInt("crashes", 0) + 1;
    if (crashes >= 3) {
        Serial.println("CRITICAL: boot loop — rolling back firmware");
        p.putUInt("crashes", 0);
        p.end();
        if (Update.canRollBack()) { Update.rollBack(); ESP.restart(); }
        Serial.println("WARNING: canRollBack() = false, no previous firmware");
    } else {
        Serial.printf("Warning: crash %lu/3\n", crashes);
        p.putUInt("crashes", crashes);
        p.end();
    }
}

// Call once WiFi is up and the device is considered stable.
void markBootSuccess() {
    Preferences p;
    p.begin("lamp", false);
    p.putUInt("crashes", 0);
    p.end();
    esp_ota_mark_app_valid_cancel_rollback();
    Serial.println("Boot verified: firmware marked as valid.");
}

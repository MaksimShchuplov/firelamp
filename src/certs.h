#pragma once

// ESP-IDF certificate bundle — ~150 root CAs maintained by Espressif.
// Linked automatically when CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y (default in arduino-esp32).
// Bundle is refreshed each time the arduino-esp32 framework package is upgraded,
// so OTA and Gemini TLS are not tied to any single hardcoded CA.
// Requires arduino-esp32 >= 3.x (espressif32 platform >= 6.x).
extern const uint8_t x509_crt_bundle_start[] asm("_binary_x509_crt_bundle_start");
extern const uint8_t x509_crt_bundle_end[]   asm("_binary_x509_crt_bundle_end");

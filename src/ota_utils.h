#pragma once
// String must be provided by the includer before this header.
// FIRMWARE_VERSION and BUILD_N are injected by get_version.py in firmware builds;
// unit tests define them before including this header.
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif
#ifndef BUILD_N
#define BUILD_N 0
#endif

inline bool isValidMd5(const String &s) {
    if (s.length() != 32) return false;
    for (unsigned i = 0; i < 32; i++) {
        char c = s[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
            return false;
    }
    return true;
}

// Uses BUILD_N for monotonic comparison when both sides are CI builds (build_n > 0);
// falls back to SHA string inequality when either side is a local dev build.
[[nodiscard]] inline bool isNewerBuild(uint32_t remoteBuildN, const String &remoteVer) {
    return (remoteBuildN > 0 && BUILD_N > 0) ? (remoteBuildN > BUILD_N)
                                              : (remoteVer != String(FIRMWARE_VERSION));
}

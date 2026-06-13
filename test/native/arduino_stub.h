#pragma once
// Minimal Arduino type stubs for native (host) unit tests.
// Provides String, integer typedefs, and FastLED saturating-math stubs.
#include <cstdint>
#include <cstdlib>
#include <string>

class String {
    std::string _s;
public:
    String() = default;
    String(const char *c) : _s(c ? c : "") {}  // NOLINT(google-explicit-constructor)

    size_t length()  const { return _s.size(); }
    bool   isEmpty() const { return _s.empty(); }
    void   reserve(size_t)  {}

    char operator[](size_t i) const { return _s[i]; }

    String& operator+=(char c)          { _s += c;   return *this; }
    String& operator+=(const char *c)   { if (c) _s += c; return *this; }
    String& operator+=(const String &o) { _s += o._s; return *this; }

    bool operator==(const String &o) const { return _s == o._s; }
    bool operator!=(const String &o) const { return _s != o._s; }

    String substring(size_t from, size_t to) const {
        return String(_s.substr(from, to - from).c_str());
    }

    const char *c_str() const { return _s.c_str(); }
};

// FastLED saturating-arithmetic stubs used by fire.cpp.
inline uint8_t qadd8(uint8_t a, uint8_t b) {
    unsigned v = (unsigned)a + b;
    return (v > 255) ? 255 : (uint8_t)v;
}
inline uint8_t qsub8(uint8_t a, uint8_t b) {
    return (b >= a) ? 0 : (uint8_t)(a - b);
}


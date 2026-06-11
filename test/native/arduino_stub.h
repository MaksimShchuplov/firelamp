#pragma once
// Minimal Arduino type stubs for native (host) unit tests.
// Provides String and integer typedefs only — no hardware APIs.
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

#pragma once
// String and uint8_t must be provided by the includer before this header.
// In firmware builds Arduino.h supplies them; in native unit tests use arduino_stub.h.

[[nodiscard]] inline String jsonEscape(const String &s) {
    String r;
    r.reserve(s.length() + 8);
    for (unsigned i = 0; i < s.length(); i++) {
        char c = s[i];
        if      (c == '"')           r += "\\\"";
        else if (c == '\\')          r += "\\\\";
        else if (c == '\n')          r += "\\n";
        else if (c == '\r')          r += "\\r";
        else if (c == '\t')          r += "\\t";
        else if ((uint8_t)c < 0x20)  { /* skip control characters */ }
        else                         r += c;
    }
    return r;
}

// Truncates s in-place to at most maxChars Unicode codepoints without splitting
// multibyte UTF-8 sequences. Continuation bytes (0x80–0xBF) count as 1 unit.
inline void truncateUtf8(String &s, int maxChars) {
    if ((int)s.length() <= maxChars) return;
    int bytePos = 0, chars = 0;
    while (bytePos < (int)s.length() && chars < maxChars) {
        uint8_t c   = (uint8_t)s[bytePos];
        int seqLen  = (c < 0x80) ? 1 : (c < 0xC0) ? 1 : (c < 0xE0) ? 2 : (c < 0xF0) ? 3 : 4;
        if (bytePos + seqLen > (int)s.length()) break;
        bytePos += seqLen; chars++;
    }
    s = s.substring(0, bytePos);
}

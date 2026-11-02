#pragma once

#include <stddef.h>
#include <stdint.h>
#include <cstring>

// Internal, JSON-safe encoding: values 0..47 use one character, other uint16s
// use three. The first base64 digit 0..15 introduces a three-character value.
inline char ScreenPixelDigit(unsigned value)
{
    return "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"[value];
}

inline int ScreenPixelValue(char c)
{
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '-') return 62;
    if (c == '_') return 63;
    return -1;
}

inline bool ScreenJsonSpace(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// Sink::put(char) returns false on allocation/capacity failure. JSON syntax
// outside pixel arrays is preserved for ArduinoJson to validate afterwards.
template<class Sink>
class ScreenJsonCompactor
{
public:
    explicit ScreenJsonCompactor(Sink &sink) : sink(sink) {}

    bool put(char c)
    {
        if (failed || c == '\0') return fail();
        if (frame)
            return pixel(c);
        if (inString)
        {
            if (static_cast<unsigned char>(c) < 32) return fail();
            if (!emit(c)) return false;
            if (escaped) { escaped = false; keyLength = 100; }
            else if (c == '\\') { escaped = true; keyLength = 100; }
            else if (c == '"')
            {
                inString = false;
                key = keyLength == 15 && !memcmp(keyText, "bitmapAnimation", 15) ? 1 :
                      keyLength == 4 && !memcmp(keyText, "data", 4) ? 2 : 0;
            }
            else if (keyLength < sizeof(keyText)) keyText[keyLength++] = c;
            else keyLength = 100;
            return true;
        }
        // Preserve token separation ("1 2" must not become "12"), while
        // collapsing formatting/padding without storing every whitespace byte.
        if (ScreenJsonSpace(c))
        {
            if (lastSpace) return true;
            lastSpace = true;
            return emit(' ');
        }
        lastSpace = false;
        if (c == '"')
        {
            inString = true; keyLength = 0; key = valueKey = 0;
        }
        else if (!ScreenJsonSpace(c))
        {
            if (c == ':') { valueKey = key; key = 0; return emit(c); }
            if (c == '{' || c == '[')
            {
                if (c == '[' && depth == dataDepth)
                {
                    frame = true; count = 0; number = false; spaced = false;
                    valueKey = key = 0;
                    return emit('"') && emit('$');
                }
                if (c == '{' && valueKey == 1 && depth <= 2) animationDepth = depth + 1;
                if (c == '[' && valueKey == 2 && depth == animationDepth) dataDepth = depth + 1;
                ++depth;
            }
            else if (c == '}' || c == ']')
            {
                --depth;
                if (depth < 0) return fail();
                if (depth < dataDepth) dataDepth = -1;
                if (depth < animationDepth) animationDepth = -1;
            }
            key = valueKey = 0;
        }
        return emit(c);
    }

    bool complete() const { return !failed && !frame && !inString && depth == 0; }

private:
    Sink &sink;
    bool failed = false, inString = false, escaped = false, frame = false;
    bool lastSpace = false;
    bool number = false, spaced = false, leadingZero = false;
    int depth = 0, animationDepth = -1, dataDepth = -1;
    unsigned key = 0, valueKey = 0, keyLength = 0, count = 0, digits = 0;
    uint32_t value = 0;
    char keyText[15];

    bool fail() { failed = true; return false; }
    bool emit(char c) { return sink.put(c) ? true : fail(); }
    bool pixel(char c)
    {
        if (ScreenJsonSpace(c)) { if (number) spaced = true; return true; }
        if (c >= '0' && c <= '9')
        {
            if (spaced || (number && leadingZero) || digits == 5) return fail();
            if (!number) { value = 0; digits = 0; leadingZero = c == '0'; number = true; }
            ++digits;
            value = value * 10 + c - '0';
            return value <= 65535 ? true : fail();
        }
        if ((c != ',' && c != ']') || !number || count == 256) return fail();
        ++count;
        if (value < 48)
        {
            if (!emit(ScreenPixelDigit(16 + value))) return false;
        }
        else if (!emit(ScreenPixelDigit(value >> 12)) ||
                 !emit(ScreenPixelDigit((value >> 6) & 63)) ||
                 !emit(ScreenPixelDigit(value & 63))) return false;
        number = spaced = false; digits = 0;
        if (c == ']') { frame = false; return emit('"'); }
        return true;
    }
};

#ifndef ANIMATION_LIMITS_H
#define ANIMATION_LIMITS_H

#include <ArduinoJson.h>
#include <cstring>

static const size_t MAX_ANIMATION_FRAMES = 28;
static const size_t MAX_ANIMATION_PIXELS = MAX_ANIMATION_FRAMES * 64;
static const size_t MAX_BITMAP_PIXELS = MATRIX_WIDTH * MATRIX_HEIGHT;
// 28 x 64 RGB565 values need at most 10,809 JSON bytes, plus metadata/topic.
static const uint16_t MAX_SCREEN_MESSAGE_SIZE = 12288;
static const uint16_t MQTT_BUFFER_SIZE = 11264;
static const uint16_t MQTT_IDLE_BUFFER_SIZE = 256;

// Decode a frame without allocating a JSON node for every pixel.
inline size_t DecodeAnimationPixels(const char *text, size_t length, uint16_t *pixels = nullptr)
{
    size_t offset = 0;
    size_t count = 0;
    while (offset < length)
    {
        while (offset < length && (text[offset] == ' ' || text[offset] == '\t' || text[offset] == '\r' || text[offset] == '\n'))
            offset++;
        if (offset == length || count == MAX_BITMAP_PIXELS || text[offset] < '0' || text[offset] > '9')
            return 0;
        unsigned long value = 0;
        size_t firstDigit = offset;
        while (offset < length && text[offset] >= '0' && text[offset] <= '9')
        {
            value = value * 10 + (text[offset++] - '0');
            if (value > 65535)
                return 0;
        }
        if (offset - firstDigit > 1 && text[firstDigit] == '0')
            return 0;
        if (pixels != nullptr)
            pixels[count] = static_cast<uint16_t>(value);
        count++;
        while (offset < length && (text[offset] == ' ' || text[offset] == '\t' || text[offset] == '\r' || text[offset] == '\n'))
            offset++;
        if (offset == length)
            return count;
        if (text[offset++] != ',' || offset == length)
            return 0;
    }
    return 0;
}

inline bool AnimationWhitespace(char c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

// Turn only frame arrays into zero-copy CSV strings in the request buffer.
// The wire format stays unchanged. The regular JSON parser validates metadata,
// while numeric pixels are decoded directly into RGB565 storage afterwards.
inline bool PackAnimationData(char *json, size_t length)
{
    int depth = 0;
    int animationDepth = -1;
    int dataDepth = -1;
    for (size_t i = 0; i < length; i++)
    {
        char c = json[i];
        if (c == '"')
        {
            size_t start = ++i;
            while (i < length && json[i] != '"')
            {
                if (json[i] == '\\')
                    i++;
                i++;
            }
            if (i >= length)
                return false;
            size_t end = i;
            size_t value = i + 1;
            while (value < length && AnimationWhitespace(json[value]))
                value++;
            if (value == length || json[value] != ':')
                continue;
            value++;
            while (value < length && AnimationWhitespace(json[value]))
                value++;
            if (value == length)
                return false;
            if (depth <= 2 && end - start == 15 && strncmp(json + start, "bitmapAnimation", 15) == 0 && json[value] == '{')
                animationDepth = depth + 1;
            else if (depth == animationDepth && end - start == 4 && strncmp(json + start, "data", 4) == 0 && json[value] == '[')
                dataDepth = depth + 1;
        }
        else if (c == '[' && depth == dataDepth)
        {
            size_t end = i + 1;
            while (end < length && json[end] != ']')
                end++;
            if (end == length || DecodeAnimationPixels(json + i + 1, end - i - 1) == 0)
                return false;
            json[i] = '"';
            json[end] = '"';
            i = end;
        }
        else if (c == '{' || c == '[')
            depth++;
        else if (c == '}' || c == ']')
        {
            depth--;
            if (depth < dataDepth)
                dataDepth = -1;
            if (depth < animationDepth)
                animationDepth = -1;
        }
    }
    return depth == 0;
}

inline bool IsValidBitmapAnimation(JsonObject &screen)
{
    if (!screen.containsKey("bitmapAnimation"))
        return true;

    JsonObject &animation = screen["bitmapAnimation"].as<JsonObject>();
    if (!animation.success())
        return false;

    int width = 8;
    int height = 8;
    if (animation.containsKey("size"))
    {
        if (!animation["size"]["width"].is<int>() || !animation["size"]["height"].is<int>())
            return false;
        width = animation["size"]["width"].as<int>();
        height = animation["size"]["height"].as<int>();
    }
    if (width <= 0 || height <= 0 || width > MATRIX_WIDTH || height > MATRIX_HEIGHT || static_cast<size_t>(width * height) > MAX_BITMAP_PIXELS)
        return false;

    JsonArray &frames = animation["data"].as<JsonArray>();
    if (!frames.success() || frames.size() == 0 || frames.size() > MAX_ANIMATION_FRAMES || frames.size() * static_cast<size_t>(width * height) > MAX_ANIMATION_PIXELS)
        return false;

    for (JsonVariant frame : frames)
    {
        if (frame.is<const char *>())
        {
            const char *pixels = frame.as<const char *>();
            if (DecodeAnimationPixels(pixels, strlen(pixels)) != static_cast<size_t>(width * height))
                return false;
            continue;
        }
        JsonArray &pixels = frame.as<JsonArray>();
        if (!pixels.success() || pixels.size() != static_cast<size_t>(width * height))
            return false;
        for (JsonVariant pixel : pixels)
        {
            if (!pixel.is<long>() || pixel.as<long>() < 0 || pixel.as<long>() > 65535)
                return false;
        }
    }
    return true;
}

#endif

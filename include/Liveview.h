#ifndef LIVEVIEW_H_
#define LIVEVIEW_H_

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <FastLED_NeoMatrix.h>
#include <CRC32.h>

#define _LIVEVIEW_PREFIX "{\"liveview\":\""
#define _LIVEVIEW_SUFFIX "\"}"
#define _LIVEVIEW_PREFIX_LENGHT 13
#define _LIVEVIEW_SUFFIX_LENGHT 2
#define _LIVEVIEW_BUFFER_LENGHT MATRIX_WIDTH *MATRIX_HEIGHT * 6 + _LIVEVIEW_PREFIX_LENGHT + _LIVEVIEW_SUFFIX_LENGHT

class Liveview
{
public:
    Liveview();
    void begin(FastLED_NeoMatrix *matrix, CRGB *leds, uint16_t interval);
    void setCallback(void (*func)(const char *, size_t));
    void loop();

protected:
    FastLED_NeoMatrix *_matrix;
    CRGB *_leds;
    uint16_t _interval;
    unsigned long _lastUpdate;
    uint32_t _lastChecksum;
    char _liveviewBuffer[_LIVEVIEW_BUFFER_LENGHT];

    void (*callbackFunction)(const char *, size_t);
    void fillBuffer();
};

#endif

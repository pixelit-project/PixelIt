#include "Liveview.h"
#include <Arduino.h>

Liveview::Liveview()
{
}

void Liveview::begin(FastLED_NeoMatrix *matrix, CRGB *leds, uint16_t interval)
{
    this->_matrix = matrix;
    _leds = leds;
    _interval = interval;
    _lastUpdate = millis();
    callbackFunction = nullptr;
}

void Liveview::setCallback(void (*func)(const char *, size_t))
{
    callbackFunction = func;
}

void Liveview::loop()
{
    if (_interval > 0 && (millis() - _lastUpdate) >= _interval)
    {
        _lastUpdate = millis();
        if (callbackFunction != nullptr)
        {
            fillBuffer();
        }
    }
}

void Liveview::fillBuffer()
{
    // set prefix
    memcpy(_liveviewBuffer, _LIVEVIEW_PREFIX, _LIVEVIEW_PREFIX_LENGHT);

    // fill buffer with led values
    for (int y = 0; y < MATRIX_HEIGHT; y++)
    {
        for (int x = 0; x < MATRIX_WIDTH; x++)
        {
            int index = this->_matrix->XY(x, y);
            sprintf(&_liveviewBuffer[(y * MATRIX_WIDTH + x) * 6 + _LIVEVIEW_PREFIX_LENGHT], "%02X%02X%02X", _leds[index].r, _leds[index].g, _leds[index].b);
        }
    }

    // set suffix
    memcpy(&_liveviewBuffer[MATRIX_HEIGHT * MATRIX_WIDTH * 6 + _LIVEVIEW_PREFIX_LENGHT], _LIVEVIEW_SUFFIX, _LIVEVIEW_SUFFIX_LENGHT);

    // rise callback
    callbackFunction(_liveviewBuffer, _LIVEVIEW_BUFFER_LENGHT);
}

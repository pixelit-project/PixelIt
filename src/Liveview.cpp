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

void Liveview::setCallback(bool (*func)(const char *, size_t, bool))
{
    callbackFunction = func;
}

void Liveview::loop(uint16_t frameInterval)
{
    // Complete queued fragments as soon as TCP has room. The sample interval
    // controls new images, not the delivery of an image already in progress.
    if (_pendingFrame && callbackFunction != nullptr)
    {
        fillBuffer();
        return;
    }
    // Sample moving frames twice per frame, bounded to 20 preview updates/s.
    uint16_t interval = _interval;
    if (frameInterval > 0 && interval > 0)
    {
        const uint16_t previewInterval = frameInterval < 100 ? 50 : frameInterval / 2;
        if (previewInterval < interval)
            interval = previewInterval;
    }
    if (interval > 0 && (millis() - _lastUpdate) >= interval)
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
    // Preserve the same bytes until every browser has received the fragments.
    if (_pendingFrame)
    {
        if (callbackFunction(_liveviewBuffer, _LIVEVIEW_BUFFER_LENGHT, true))
        {
            _lastChecksum = _pendingChecksum;
            _pendingFrame = false;
        }
        return;
    }
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

    // calculate checksum
    uint32_t newChecksum = CRC32::calculate((byte *)_liveviewBuffer, _LIVEVIEW_BUFFER_LENGHT);
    if (_lastChecksum != newChecksum)
    {
        // Serial.printf("Checksum (new/old): 0x%08X/0x%08X\n", newChecksum, _lastChecksum);
        // Keep retrying an unchanged final image if a slow browser was full.
        if (callbackFunction(_liveviewBuffer, _LIVEVIEW_BUFFER_LENGHT, false))
            _lastChecksum = newChecksum;
        else
        {
            _pendingChecksum = newChecksum;
            _pendingFrame = true;
        }
    }
}
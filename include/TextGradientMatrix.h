#pragma once

#include <FastLED_NeoMatrix.h>

class TextGradientMatrix : public FastLED_NeoMatrix
{
public:
    using FastLED_NeoMatrix::FastLED_NeoMatrix;

    // The palette lives only for the synchronous glyph draw.
    void setGlyphPalette(const uint16_t *palette, int8_t xAxis = 0, int8_t yAxis = 0, int16_t offset = 0)
    {
        glyphPalette = palette;
        glyphXAxis = xAxis;
        glyphYAxis = yAxis;
        glyphOffset = offset;
    }

    void writePixel(int16_t x, int16_t y, uint16_t color) override
    {
        if (glyphPalette)
        {
            if (x < 0 || y < 0 || x >= width() || y >= height())
                return;
            color = glyphPalette[x * glyphXAxis + y * glyphYAxis + glyphOffset];
        }
        FastLED_NeoMatrix::writePixel(x, y, color);
    }

private:
    const uint16_t *glyphPalette = nullptr;
    int8_t glyphXAxis = 0, glyphYAxis = 0;
    int16_t glyphOffset = 0;
};

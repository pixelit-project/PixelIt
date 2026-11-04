#include <Arduino.h>
#include "UpdateVisuals.h"

uint16_t UpdateArrowPixel(uint8_t phase, uint8_t x, uint8_t y)
{
    static const uint8_t shape[8] PROGMEM = {
        0x18, 0x3C, 0x7E, 0xFF, 0x18, 0x18, 0x18, 0x18};
    static const uint16_t colors[8] PROGMEM = {
        0x801F, 0x981F, 0xB81F, 0xE01C, 0xF979, 0xFA97, 0xE01C, 0xB01F};
    if (x >= 8 || y >= 8 || !(pgm_read_byte(&shape[y]) & (0x80 >> x)))
        return 0;
    return pgm_read_word(&colors[(y + 8 - (phase % 8)) % 8]);
}


uint16_t LogoGradientColor(uint16_t position, uint16_t width, uint8_t brightness)
{
    const uint16_t span = width > 1 ? width - 1 : 1;
    if (position > span) position = span;
    const uint16_t r = (128 + 127UL * position / span) * brightness / 255;
    const uint16_t g = (65UL * position / span) * brightness / 255;
    const uint16_t b = (255 - 65UL * position / span) * brightness / 255;
    return ((r & 248) << 8) | ((g & 252) << 3) | (b >> 3);
}

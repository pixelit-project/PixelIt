#pragma once
#include <stdint.h>

// Fixed arrow silhouette; only the eight-phase logo-color wave moves.
uint16_t UpdateArrowPixel(uint8_t phase, uint8_t x, uint8_t y);

// Same violet-to-pink gradient for boot/reboot lettering and WiFi trail.
uint16_t LogoGradientColor(uint16_t position, uint16_t width, uint8_t brightness = 255);

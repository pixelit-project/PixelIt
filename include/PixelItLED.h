#pragma once

// FastLED 3.7.0 uses WAIT_TIME both for reset and its interrupt deadline.
// Keep these coupled: a longer reset must NOT permit longer inter-pixel gaps.
#if defined(ESP8266)
#define FASTLED_ALLOW_INTERRUPTS 1
#define INTERRUPT_THRESHOLD 260
#endif

#include <FastLED.h>

#if defined(ESP8266)
// Preserve NEOPIXEL bit timing and GRB order. Older WS2812B can latch after
// 50 us low; newer revisions need up to 300 us reset before another frame.
// With INTERRUPT_THRESHOLD=260 the driver's deadline is 300-260 = 40 us.
// Interrupt-stretched frames are retried, rather than accepted as successful.
template<uint8_t DATA_PIN>
class PixelItLEDController : public ClocklessController<DATA_PIN,
    C_NS(250), C_NS(625), C_NS(375), GRB, 0, false, 300> {};
#else
template<uint8_t DATA_PIN>
class PixelItLEDController : public NEOPIXEL<DATA_PIN> {};
#endif

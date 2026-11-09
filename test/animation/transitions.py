"""Check the screen transition renderer with a serpentine matrix and simulated time."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
stub = r'''
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>
using std::min;
using std::max;
#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 8

struct CRGB {
    uint32_t value;
    uint8_t r = 0, g = 0, b = 0;
    CRGB() : value(0) {}
    CRGB(uint32_t value) : value(value), r(value) {}
    CRGB(uint8_t r, uint8_t g, uint8_t b) : value(r), r(r), g(g), b(b) {}
    static const CRGB Black, White;
};
extern CRGB leds[256];
struct JsonVariant {
    uint16_t value;
    template <typename T> T as() const { return value; }
};
using JsonArray = std::vector<JsonVariant>;
bool PrepareScreenTransition(const char *, CRGB = CRGB::White, bool = true);
constexpr unsigned ClockFallbackLastEffect = 11;
constexpr unsigned ClockDateTransitionAuto = 12;
extern uint8_t clockColorR, clockColorG, clockColorB;
extern long randomChoice;
inline long random(long first, long last) {
    return std::max(first, std::min(last - 1, randomChoice));
}
inline uint16_t ColorWheel(uint8_t pos, int) { return pos + 1; }
extern bool firmwareUpdateDisplayActive;
extern unsigned long animateBMPPrevMillis, scrollTextPrevMillis, animationLastRenderedMillis;
extern uint32_t tick;
inline uint32_t millis() { return tick; }

struct Matrix {
    int shown = 0;
    int XY(int x, int y) const {
        return y * 32 + (y % 2 ? 31 - x : x);
    }
    void show() { ++shown; }
    void drawPixel(int x, int y, uint16_t color) {
        if (x >= 0 && x < 32 && y >= 0 && y < 8)
            leds[XY(x, y)] = CRGB(color);
    }
    void fillRect(int x, int y, int w, int h, uint16_t color) {
        for (int row = y; row < y + h; ++row)
            for (int col = x; col < x + w; ++col)
                drawPixel(col, row, color);
    }
    void drawFastVLine(int x, int y, int h, uint16_t color) {
        fillRect(x, y, 1, h, color);
    }
    uint16_t Color(uint8_t r, uint8_t, uint8_t) { return r; }
    void clear() { fillRect(0, 0, 32, 8, 0); }
};
extern Matrix *matrix;
'''
checks = r'''
#include <cassert>
#include "ScreenTransition.cpp"

const CRGB CRGB::Black = {0};
const CRGB CRGB::White = {255};
CRGB leds[256];
bool firmwareUpdateDisplayActive = false;
uint8_t clockColorR = 255, clockColorG = 255, clockColorB = 255;
long randomChoice = 1;
unsigned long animateBMPPrevMillis, scrollTextPrevMillis, animationLastRenderedMillis;
uint32_t tick;
Matrix display;
Matrix *matrix = &display;

void fill(uint32_t base) {
    for (int i = 0; i < 256; ++i)
        leds[i] = {base + static_cast<uint32_t>(i)};
}

uint32_t pixel(int x, int y) {
    return leds[display.XY(x, y)].value;
}

int main() {
    assert(!PrepareScreenTransition(nullptr));
    assert(!PrepareScreenTransition("unknown"));
    assert(!PrepareScreenTransition("slide", CRGB::White, false));
    for (const char *name : {"fade", "coloredBarWipe", "zigzagWipe", "bitmapWipe"}) {
        fill(100);
        tick = UINT32_MAX - 100;
        const uint32_t start = tick;
        if (strcmp(name, "bitmapWipe") == 0) {
            assert(!PrepareBitmapWipe({}, 1, true));
            assert(!PrepareBitmapWipe({}, 33, true));
            assert(PrepareBitmapWipe(JsonArray(8, {42}), 1, true));
        } else {
            assert(PrepareScreenTransition(name));
        }
        fill(200);
        StartScreenTransition();
        assert(pixel(0, 0) == 100);
        tick = start + 140;
        const int shown = display.shown;
        assert(ServiceScreenTransition());
        assert(display.shown == shown + 1);
        if (strcmp(name, "fade") == 0)
            assert(leds[display.XY(0, 0)].r < 100);
        else
            assert(pixel(0, 0) == 0);
        assert(ServiceScreenTransition());
        assert(display.shown == shown + 1);
        tick = start + 2300;
        assert(ServiceScreenTransition());
        assert(!ScreenTransitionActive() && pixel(0, 0) == 200);
    }
    fill(50);
    assert(PrepareScreenTransition("fade", CRGB::White, false));
    StartScreenTransition();
    tick += 600;
    assert(ServiceScreenTransition() && pixel(0, 0) == 50);
    assert(PrepareScreenTransition("coloredBarWipe", CRGB::White, false));
    StartScreenTransition();
    tick += 900;
    assert(ServiceScreenTransition() && pixel(0, 0) == 0);

    assert(PrepareBitmapWipe(JsonArray(8, {42}), 1, false));
    StartScreenTransition();
    tick += 900;
    assert(ServiceScreenTransition());
    assert(pixel(30, 0) == 0 && pixel(31, 0) == 42);

    assert(!PrepareScreenTransition("verticalSlide", CRGB::White, false));
    assert(!PrepareClockTransition(0, false, false));
    for (int mode : {0, 1, 2, 3}) {
        fill(100);
        if (mode == 0)
            assert(PrepareScreenTransition("verticalSlide"));
        else if (mode == 1)
            assert(PrepareClockTransition(11, false, true));
        else if (mode == 2)
            assert(PrepareClockTransition(ClockDateTransitionAuto, false, true));
        else {
            randomChoice = 10;
            assert(PrepareClockTransition(4, false, false));
        }
        const int rows = mode == 1 || mode == 2 ? 7 : 8;
        fill(1000);
        StartScreenTransition();
        const uint32_t start = tick;
        for (int step : {1, 3, 6}) {
            tick = start + 35 * step;
            assert(ServiceScreenTransition());
            assert(pixel(0, 0) == 1000 + display.XY(0, rows - step));
            assert(pixel(1, step) == 100 + display.XY(1, 0));
            if (rows == 7)
                assert(pixel(1, 7) == 100 + display.XY(1, 7));
        }
        tick = start + 35 * rows;
        assert(ServiceScreenTransition() && !ScreenTransitionActive());
        assert(pixel(1, 7) == 1000 + display.XY(1, 7));
    }
    fill(100);
    assert(PrepareClockTransition(ClockDateTransitionAuto, true, false));
    fill(200);
    StartScreenTransition();
    const int shown = display.shown;
    tick += 29;
    assert(ServiceScreenTransition() && display.shown == shown);
    tick += 1;
    assert(ServiceScreenTransition() && display.shown == shown + 1);
    CancelScreenTransition();

    for (const char *name : {"curtain", "pixelDissolve", "slide", "blinds", "crt", "glitch"}) {
        fill(1);
        tick = UINT32_MAX - 100;
        display.shown = 0;
        assert(PrepareScreenTransition(name));
        fill(1001);
        ShowScreenContentFrame();
        assert(ServiceScreenTransition());
        assert(display.shown == 0);
        StartScreenTransition();
        assert(pixel(0, 0) == 1);
        const uint32_t start = tick;
        for (int step : {1, 3, 10, 16}) {
            tick = start + 24 * step;
            const int shown = display.shown;
            assert(ServiceScreenTransition());
            assert(display.shown == shown + 1);
            if (step != 16) {
                assert(ServiceScreenTransition());
                assert(display.shown == shown + 1);
            }
            if (strcmp(name, "pixelDissolve") == 0 && step == 1)
                assert(pixel(0, 0) == 1001 && pixel(1, 0) == 2);
            if (strcmp(name, "blinds") == 0 && step == 3)
                assert(pixel(4, 0) == 1005 && pixel(5, 0) == 6);
            if (strcmp(name, "glitch") == 0 && step == 1)
                assert(pixel(0, 0) == 3 && pixel(0, 1) == 64);
            if (strcmp(name, "slide") == 0 && step == 10) {
                assert(pixel(0, 1) == 44);
                assert(pixel(12, 1) == 1064);
            }
            if (strcmp(name, "curtain") == 0 && step == 10)
                assert(pixel(6, 0) == 0 && pixel(15, 0) == 1016);
            if (strcmp(name, "crt") == 0 && step == 10)
                assert(pixel(0, 0) == 0 && pixel(0, 4) == 1129);
        }
        assert(!ServiceScreenTransition());
        assert(animateBMPPrevMillis == tick && scrollTextPrevMillis == tick);
        assert(animationLastRenderedMillis == tick);
        for (uint32_t i = 0; i < 256; ++i)
            assert(leds[i].value == 1001 + i);
    }

    assert(PrepareScreenTransition("slide"));
    fill(2001);
    StartScreenTransition();
    tick += 24;
    assert(ServiceScreenTransition());
    const uint32_t visible = pixel(0, 0);
    assert(PrepareScreenTransition("blinds"));
    fill(3001);
    StartScreenTransition();
    assert(pixel(0, 0) == visible);
    CancelScreenTransition();
    tick += 500;
    assert(!ServiceScreenTransition() && pixel(0, 0) == visible);
}
'''
with tempfile.TemporaryDirectory() as tmp:
    path = Path(tmp)
    (path / "FirmwareState.h").write_text(stub)
    (path / "transitions.cpp").write_text(checks)
    executable = path / "transitions"
    compiler = shutil.which("clang++") or shutil.which("c++")
    subprocess.run([compiler, "-std=c++11", "-I", tmp, "-I", str(root / "src"),
                    str(path / "transitions.cpp"), "-o", str(executable)], check=True)
    subprocess.run([str(executable)], check=True)
print("Screen transition regression checks passed")

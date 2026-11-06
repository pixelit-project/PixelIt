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
using std::min;
#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 8

struct CRGB {
    uint32_t value;
    static const CRGB Black;
};
extern CRGB leds[256];
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
};
extern Matrix *matrix;
'''
checks = r'''
#include <cassert>
#include "ScreenTransition.cpp"

const CRGB CRGB::Black = {0};
CRGB leds[256];
bool firmwareUpdateDisplayActive = false;
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

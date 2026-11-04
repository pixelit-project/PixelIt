#!/usr/bin/env python3
"""Exercise the actual OTA display callback with a bounded fake matrix."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'src/UpdateVisuals.cpp').read_text() + '\n' + (root / 'src/FirmwareUpdateDisplay.cpp').read_text()
source = '\n'.join(line for line in source.splitlines() if not line.startswith('#include'))
program = r'''
#include <cassert>
#include "UpdateVisuals.h"
#include <cstdint>
#include <cstddef>
#include <cstdlib>
#include <algorithm>
#define PROGMEM
#define pgm_read_word(address) (*(address))
#define pgm_read_byte(address) (*(address))
#define MATRIX_WIDTH 32
#define MATRIX_HEIGHT 8
enum {UPLOAD_FILE_START, UPLOAD_FILE_WRITE, UPLOAD_FILE_END, UPLOAD_FILE_ABORTED};
bool firmwareUpdateDisplayActive = false;
int currentMatrixBrightness = 0;
unsigned long tick = 0;
unsigned long millis() { return tick; }
void delay(unsigned long duration) { tick += duration; }
struct Matrix {
    int shows = 0, bar = 0, brightness = 0;
    uint16_t pixels[8][32] = {};
    uint16_t Color(int r, int g, int b) { return ((r & 248) << 8) | ((g & 252) << 3) | (b >> 3); }
    void clear() { for (auto &row : pixels) std::fill(row, row + 32, 0); }
    void setBrightness(int value) { brightness = value; }
    void drawPixel(int x, int y, uint16_t color) {
        assert(x >= 0 && x < 32 && y >= 0 && y < 8);
        pixels[y][x] = color;
    }
    void drawFastHLine(int x, int y, int width, uint16_t color) {
        assert(width >= 0 && width <= 32);
        bar = width;
        for (int i = 0; i < width; ++i) drawPixel(x + i, y, color);
    }
    int filled() {
        int count = 0;
        for (int x = 10; x < 32; ++x)
            if (pixels[6][x] != 0 && pixels[6][x] != Color(12, 0, 20)) ++count;
        return count;
    }
    void show() {
        for (int x = 10; x < 32; ++x) assert(pixels[6][x] == pixels[7][x]);
        ++shows;
    }
} display;
Matrix *matrix = &display;
''' + source + r'''
int main() {
    FirmwareUpdateUpload(UPLOAD_FILE_START, 0, 1000000, false);
    assert(firmwareUpdateDisplayActive && display.shows == 1 && display.filled() == 0);
    assert(display.brightness > 0); // Sleeping display must still show the update.
    tick = 50;
    FirmwareUpdateUpload(UPLOAD_FILE_WRITE, 500000, 1000000, false);
    assert(display.shows == 1); // Bound LED transfer overhead.
    tick = 100;
    FirmwareUpdateUpload(UPLOAD_FILE_WRITE, 500000, 1000000, false);
    assert(display.shows == 2 && display.filled() == 10);
    tick = 200;
    FirmwareUpdateUpload(UPLOAD_FILE_WRITE, 1000000, 1000000, false);
    assert(display.filled() == 19); // Never claim success before finalization.
    FirmwareUpdateUpload(UPLOAD_FILE_END, 1000000, 1000000, false);
    assert(display.filled() == 20 && firmwareUpdateDisplayActive);
    const auto beforeReboot = tick;
    const auto beforeRebootShows = display.shows;
    FirmwareUpdateReboot();
    assert(tick - beforeReboot == 3600 && display.shows == beforeRebootShows + 17);
    // REBOOT uses the full display, no arrow/bar, with a violet/pink gradient.
    for (int y = 0; y < 8; ++y)
        for (int x = 0; x < 32; ++x)
            if (x < 4 || x > 26 || y == 0 || y >= 6) assert(display.pixels[y][x] == 0);
    assert(display.pixels[1][4] == display.Color(128, 0, 255));
    assert(display.pixels[1][26] == display.Color(255, 65, 190));
    // A new request resets throttling and failure state.
    FirmwareUpdateUpload(UPLOAD_FILE_START, 0, 100, false);
    FirmwareUpdateUpload(UPLOAD_FILE_END, 100, 100, true);
    assert(!firmwareUpdateDisplayActive && errorShown);
    const auto failureTime = tick;
    const auto failureShows = display.shows;
    FirmwareUpdateReboot();
    assert(tick == failureTime && display.shows == failureShows);
    int shown = display.shows;
    tick += 100;
    FirmwareUpdateUpload(UPLOAD_FILE_WRITE, 100, 100, true);
    assert(display.shows == shown);
    FirmwareUpdateUpload(UPLOAD_FILE_START, 0, 0, false);
    FirmwareUpdateUpload(UPLOAD_FILE_ABORTED, 0, 0, false);
    assert(!firmwareUpdateDisplayActive && errorShown);
    FirmwareUpdateUpload(UPLOAD_FILE_START, 0, 0, false);
    tick += 100;
    FirmwareUpdateUpload(UPLOAD_FILE_WRITE, 1234, 0, false);
    assert(display.filled() == 0); // Unknown length must not divide by zero.
    FirmwareUpdateUpload(UPLOAD_FILE_END, 1234, 0, false);
    assert(display.filled() == 20);
    FirmwareUpdateUpload(UPLOAD_FILE_START, 0, 0xffffffff, false);
    tick += 100;
    FirmwareUpdateUpload(UPLOAD_FILE_WRITE, 0xfffffffe, 0xffffffff, false);
    assert(display.filled() == 19); // Multiplication must not overflow on 32-bit targets.
    shown = display.shows;
    tick += 400;
    FirmwareUpdateUpload(UPLOAD_FILE_WRITE, 0xfffffffe, 0xffffffff, false);
    assert(display.shows == shown + 1); // Animate even without a whole-percent change.
    shown = display.shows;
    tick += 100;
    FirmwareUpdateUpload(UPLOAD_FILE_WRITE, 0xfffffffe, 0xffffffff, false);
    assert(display.shows == shown + 1); // Bar light moves between 200 ms icon frames.
}
'''
with tempfile.TemporaryDirectory() as temp:
    path = Path(temp)
    (path / 'display.cpp').write_text(program)
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-I', str(root / 'include'), str(path / 'display.cpp'),
                    '-o', str(path / 'display')], check=True)
    subprocess.run([str(path / 'display')], check=True)
print('OTA matrix progress tests passed')

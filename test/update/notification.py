#!/usr/bin/env python3
"""Check shared arrow geometry, colors and bounded notification construction."""
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
json_include = next((root / '.pio/libdeps').glob('*/ArduinoJson/src'))
source = '\n'.join((root / 'src' / name).read_text()
                   for name in ('UpdateVisuals.cpp', 'UpdateScreen.cpp'))
source = '\n'.join(line for line in source.splitlines() if not line.startswith('#include'))
program = r'''
#include <cassert>
#include "UpdateVisuals.h"
#include <cstdint>
#include <cstring>
#include <ArduinoJson.h>
#include "AnimationLimits.h"
#define PROGMEM
#define pgm_read_word(address) (*(address))
#define pgm_read_byte(address) (*(address))
''' + source + r'''
int main() {
    const uint8_t expectedShape[] = {0x18,0x3C,0x7E,0xFF,0x18,0x18,0x18,0x18};
    const uint16_t expectedColors[] = {0x801F,0x981F,0xB81F,0xE01C,0xF979,0xFA97,0xE01C,0xB01F};
    StaticJsonBuffer<4096> storage;
    JsonObject &root = storage.createObject();
    assert(BuildUpdateScreenJSON(root));
    assert(!strcmp(root["text"]["textString"], "New FW available"));
    assert(root["text"]["position"]["x"] == 8);
    assert(root["bitmapAnimation"]["animationDelay"] == 200);
    JsonArray &frames = root["bitmapAnimation"]["data"];
    assert(frames.size() == 8);
    for (uint8_t phase = 0; phase < 8; ++phase) {
        uint16_t pixels[64];
        const char *encoded = frames[phase];
        assert(encoded && DecodeAnimationPixels(encoded, strlen(encoded), pixels) == 64);
        for (uint8_t y = 0; y < 8; ++y)
            for (uint8_t x = 0; x < 8; ++x) {
                const uint16_t expected = (expectedShape[y] & (0x80 >> x)) ?
                    expectedColors[(y + 8 - phase) % 8] : 0;
                assert(UpdateArrowPixel(phase, x, y) == expected);
                assert(pixels[y * 8 + x] == expected);
            }
    }
    assert(UpdateArrowPixel(0, 8, 0) == 0);
    assert(UpdateArrowPixel(0, 0, 8) == 0);
    // JSON heap exhausted before creating notification: reject before CreateFrames.
    StaticJsonBuffer<64> tiny;
    JsonObject &unavailable = tiny.createObject();
    assert(!BuildUpdateScreenJSON(unavailable));
    // Partial construction failure must also be detected, not just root.success().
    StaticJsonBuffer<600> partial;
    JsonObject &incomplete = partial.createObject();
    assert(!BuildUpdateScreenJSON(incomplete));
}
'''
with tempfile.TemporaryDirectory() as temp:
    path = Path(temp)
    (path / 'notification.cpp').write_text(program)
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-DMATRIX_WIDTH=32',
                    '-DMATRIX_HEIGHT=8', '-I', str(json_include), '-I', str(root / 'include'),
                    str(path / 'notification.cpp'), '-o', str(path / 'notification')], check=True)
    subprocess.run([str(path / 'notification')], check=True)
print('Shared update arrow and notification tests passed')

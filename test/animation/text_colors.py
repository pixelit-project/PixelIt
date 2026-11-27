"""Host regression test for screen text color parsing and rendering."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / "src/Display.cpp").read_text()
renderer = source[source.index("namespace\n{"):source.index("} // namespace") + len("} // namespace")]
matrix_header = (root / "include/TextGradientMatrix.h").read_text()
matrix_class = matrix_header[matrix_header.index("class TextGradientMatrix"):]
json_include = next((root / ".pio/libdeps").glob("*/ArduinoJson/src"))

stubs = r"""
#include <ArduinoJson.h>
#include <cassert>
#include <string>
#include <vector>

using String = std::string;
constexpr int MATRIX_WIDTH = 32, MATRIX_HEIGHT = 8;
struct CRGB {
    uint8_t r, g, b;
    CRGB() = default;
    CRGB(unsigned rgb) : r(rgb >> 16), g(rgb >> 8), b(rgb) {}
    CRGB(uint8_t r, uint8_t g, uint8_t b) : r(r), g(g), b(b) {}
};
struct CHSV {
    uint8_t hue;
    CHSV(uint8_t hue, int, int) : hue(hue) {}
};
void hsv2rgb_rainbow(CHSV hsv, CRGB &rgb) { rgb = CRGB(0, hsv.hue, 255); }
CRGB blend(CRGB from, CRGB to, uint8_t amount) {
    if (amount == 0) return from;
    if (amount == 255) return to;
    return CRGB(0x808080);
}
struct FastLED_NeoMatrix {
    uint16_t color = 0;
    unsigned prints = 0;
    int16_t cursorX = 0, cursorY = 0;
    int16_t width() { return MATRIX_WIDTH; }
    int16_t height() { return MATRIX_HEIGHT; }
    int16_t getCursorX() { return cursorX; }
    int16_t getCursorY() { return cursorY; }
    String written;
    std::vector<uint16_t> colors;
    static uint16_t Color(uint8_t r, uint8_t g, uint8_t b) {
        return ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3);
    }
    void setTextColor(uint16_t value) { color = value; }
    void write(uint8_t value) {
        written += value;
        if (value == '?') {
            writePixel(cursorX, cursorY, color);
            writePixel(cursorX + 1, cursorY, color);
            writePixel(cursorX, cursorY + 1, color);
            writePixel(cursorX + 1, cursorY + 1, color);
        } else {
            colors.push_back(color);
        }
        cursorX += 2;
    }
    virtual void writePixel(int16_t x, int16_t y, uint16_t color) {
        if (x >= 0 && y >= 0 && x < width() && y < height())
            colors.push_back(color);
    }
    void print(const String &) { ++prints; }
};
"""

matrix_instance = "TextGradientMatrix display; TextGradientMatrix *matrix = &display;\n"
checks = r"""
int main() {
    DynamicJsonBuffer buffer;
    const auto &options = buffer.parseObject(R"({
        "gradientEnd": "#0000ff",
        "characterColors": ["#000000", null, "#fF0", "#00FF00"]
    })");
    TextColors colors{};
    colors.base = CRGB(0xff0000);
    ReadTextColors(colors, options, 5);
    const auto expected = std::vector<uint16_t>{0, 0x8410, 0x8410, 0x07e0, 0x001f};
    PrintText("A BCD", colors);
    assert(display.written == "A BCD" && display.colors == expected);
    assert(display.color == 0xf800);

    // A scrolling frame reuses the copied colors after the JSON has expired.
    scrollColors = colors;
    buffer.clear();
    display.colors.clear();
    PrintText("A BCD", scrollColors);
    assert(display.colors == expected);

    display.colors.clear();
    PrintText("x", TextColors{CRGB(0xff0000), CRGB(0x0000ff), true});
    assert(display.colors == std::vector<uint16_t>{0xf800});

    const auto &rainbow = buffer.parseObject(R"({"rainbow": true, "gradientEnd": "#00ff00"})");
    colors = TextColors{};
    ReadTextColors(colors, rainbow, 2);
    display.colors.clear();
    PrintText("AB", colors);
    assert((display.colors == std::vector<uint16_t>{0x001f, 0x041f}));

    auto &many = buffer.createObject();
    auto &array = many.createNestedArray("characterColors");
    for (unsigned i = 0; i < 65; ++i) array.add("#ffffff");
    colors = TextColors{};
    ReadTextColors(colors, many, 65);
    display.colors.clear();
    PrintText(String(65, 'x'), colors);
    assert(display.colors[63] == 0xffff && display.colors[64] == 0);

    const auto checkDirection = [&](const char *direction, std::vector<uint16_t> expected) {
        DynamicJsonBuffer json;
        auto &options = json.createObject();
        options["gradientDirection"] = direction;
        options["gradientEnd"] = "#0000ff";
        colors = TextColors{};
        colors.base = CRGB(0xff0000);
        colors.boundsWidth = 2;
        colors.boundsHeight = 2;
        ReadTextColors(colors, options, 1);
        display.colors.clear();
        display.cursorX = 7;
        display.cursorY = 2;
        PrintText("?", colors);
        assert(display.colors == expected);

        // Repainting at a different X keeps the gradient attached to the text.
        display.colors.clear();
        display.cursorX = 2;
        PrintText("?", colors);
        assert(display.colors == expected);
    };
    checkDirection("leftToRight", {0xf800, 0x001f, 0xf800, 0x001f});
    checkDirection("rightToLeft", {0x001f, 0xf800, 0x001f, 0xf800});
    checkDirection("topToBottom", {0xf800, 0xf800, 0x001f, 0x001f});
    checkDirection("bottomToTop", {0x001f, 0x001f, 0xf800, 0xf800});
    checkDirection("topLeftToBottomRight", {0xf800, 0x8410, 0x8410, 0x001f});
    checkDirection("bottomRightToTopLeft", {0x001f, 0x8410, 0x8410, 0xf800});
    checkDirection("topRightToBottomLeft", {0x8410, 0xf800, 0x001f, 0x8410});
    checkDirection("bottomLeftToTopRight", {0x8410, 0x001f, 0xf800, 0x8410});
    checkDirection("invalid", {0xf800, 0xf800, 0xf800, 0xf800});

    DynamicJsonBuffer directional;
    auto &overrides = directional.parseObject(R"({
        "gradientDirection": "leftToRight", "gradientEnd": "#0000ff",
        "characterColors": ["#00ff00", null]
    })");
    colors = TextColors{};
    colors.base = CRGB(0xff0000);
    colors.boundsWidth = 4;
    colors.boundsHeight = 2;
    ReadTextColors(colors, overrides, 2);
    display.colors.clear();
    display.cursorX = 0;
    PrintText("??", colors);
    assert((display.colors == std::vector<uint16_t>{
        0x07e0, 0x07e0, 0x07e0, 0x07e0, 0x8410, 0x001f, 0x8410, 0x001f
    }));
    display.colors.clear();
    display.cursorX = -3;
    PrintText("??", colors);
    assert((display.colors == std::vector<uint16_t>{0x001f, 0x001f}));

    auto &motion = directional.parseObject(R"({"gradientEnd":"#0000ff","gradientCycleMs":1800})");
    colors = TextColors{};
    ReadTextColors(colors, motion, 2);
    assert(colors.cycleMs == 1800 && colors.xAxis == 1 && colors.yAxis == 0);
    for (const char *invalid : {"true", "0", "249", "60001", "500.5", "\"1800\""}) {
        DynamicJsonBuffer json;
        const String input = String("{\"rainbow\":true,\"gradientCycleMs\":") + invalid + "}";
        colors = TextColors{};
        ReadTextColors(colors, json.parseObject(input.c_str()), 2);
        assert(colors.cycleMs == 0 && colors.xAxis == 0);
    }
    assert(MovingColorAmount(0, 0) == 0);
    assert(MovingColorAmount(64, 0) == MovingColorAmount(128, 64));
    assert(MovingColorAmount(1, 255) == MovingColorAmount(2, 0));

    PrintText("solid", TextColors{});
    assert(display.prints == 1);
}
"""

with tempfile.TemporaryDirectory() as temp:
    cpp = Path(temp) / "text_colors.cpp"
    binary = Path(temp) / "text_colors"
    cpp.write_text(stubs + matrix_class + matrix_instance + renderer + checks)
    subprocess.run([shutil.which("clang++") or "c++", "-std=c++17", "-I", str(json_include),
                    str(cpp), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
print("Text color parsing and rendering tests passed")

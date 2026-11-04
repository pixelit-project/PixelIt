#include "FirmwareState.h"
#include "FirmwareUpdateDisplay.h"

namespace
{
unsigned long lastDraw = 0;
int lastPercent = -1;
bool errorShown = false;
uint8_t lastArrowFrame = 0;
uint8_t lastBarFrame = 0;
unsigned long uploadStarted = 0;

// Three by five glyphs avoid changing the normal renderer's font/cursor state.
void Glyph(int x, uint16_t bits, uint16_t color, bool reboot = false)
{
    for (int y = 0; y < 5; ++y)
        for (int col = 0; col < 3; ++col)
            if (bits & (1U << (14 - y * 3 - col)))
            {
                if (reboot)
                {
                    const int offset = x + col - (MATRIX_WIDTH - 23) / 2;
                    color = LogoGradientColor(offset, 23);
                }
                matrix->drawPixel(x + col, y + (reboot ? 1 : 0), color);
            }
}

void DrawProgress(int percent, bool failed)
{
    static const uint16_t digits[] = {
        0x7B6F, 0x2492, 0x73E7, 0x73CF, 0x5BC9,
        0x79CF, 0x79EF, 0x7249, 0x7BEF, 0x7BCF};
    matrix->clear();
    // Make updates visible even when the normal screen is asleep/dimmed.
    matrix->setBrightness(currentMatrixBrightness > 0 ? currentMatrixBrightness : 16);
    const uint16_t color = failed ? matrix->Color(255, 0, 0) : matrix->Color(245, 235, 255);
    if (failed)
    {
        const int x = (MATRIX_WIDTH - 11) / 2;
        Glyph(x, 0x79E7, color); // E
        Glyph(x + 4, 0x6B6D, color); // R
        Glyph(x + 8, 0x6B6D, color);
    }
    else
    {
        const uint8_t frame = (millis() / 200) % 8;
        for (int y = 0; y < 8; ++y)
            for (int col = 0; col < 8; ++col)
            {
                uint16_t pixel = UpdateArrowPixel(frame, col, y);
                const unsigned long elapsed = millis() - uploadStarted;
                if (elapsed < 800)
                {
                    const int fade = 40 + 60 * elapsed / 800;
                    pixel = matrix->Color(((pixel >> 11) & 31) * 255 / 31 * fade / 100,
                                          ((pixel >> 5) & 63) * 255 / 63 * fade / 100,
                                          (pixel & 31) * 255 / 31 * fade / 100);
                }
                matrix->drawPixel(col, y, pixel);
            }
        const int count = percent >= 100 ? 3 : (percent >= 10 ? 2 : 1);
        int x = 11 + (MATRIX_WIDTH - 12 - (count * 4 + 3)) / 2;
        if (count == 3) { Glyph(x, digits[1], color); x += 4; }
        if (count >= 2) { Glyph(x, digits[(percent / 10) % 10], color); x += 4; }
        Glyph(x, digits[percent % 10], color);
        Glyph(x + 4, 0x52A5, color); // %
    }
    const int start = failed ? 0 : 11;
    const int width = failed ? MATRIX_WIDTH : MATRIX_WIDTH - 12;
    const int filled = failed ? width : width * percent / 100;
    const int lightPosition = filled ? (millis() / 100) % filled : 0;
    for (int x = 0; x < width; ++x)
    {
        uint16_t pixelColor = matrix->Color(12, 0, 20);
        if (x < filled)
        {
            const int distance = abs(x - lightPosition);
            const int glow = distance < 3 ? (3 - distance) * 65 : 0;
            pixelColor = failed ? color : matrix->Color(128 + 127 * glow / 300,
                                                       50 * glow / 300, 255 - 55 * glow / 300);
        }
        for (int y = MATRIX_HEIGHT - 2; y < MATRIX_HEIGHT; ++y)
            matrix->drawPixel(start + x, y, pixelColor);
    }
    matrix->show();
}
}

void FirmwareUpdateUpload(int status, size_t received, size_t requestBytes, bool failed)
{
    if (status == UPLOAD_FILE_START)
    {
        firmwareUpdateDisplayActive = true;
        uploadStarted = millis();
        lastPercent = -1;
        errorShown = false;
        lastArrowFrame = (millis() / 200) % 8;
        lastBarFrame = (millis() / 100) % (MATRIX_WIDTH - 12);
    }
    failed = failed || status == UPLOAD_FILE_ABORTED;
    // Multipart length includes boundaries/headers: approximate during upload.
    // Only a successfully finalized image may display 100%.
    int percent = requestBytes ? static_cast<int>((uint64_t(received) * 100) / requestBytes) : 0;
    if (percent > 99) percent = 99;
    const bool finished = status == UPLOAD_FILE_END && !failed;
    if (finished) percent = 100;
    const unsigned long nowMillis = millis();
    if ((failed && !errorShown) || finished || status == UPLOAD_FILE_START ||
        (!failed && (percent != lastPercent || (nowMillis / 200) % 8 != lastArrowFrame ||
                     (nowMillis / 100) % (MATRIX_WIDTH - 12) != lastBarFrame) &&
         nowMillis - lastDraw >= 100))
    {
        DrawProgress(percent, failed);
        lastDraw = nowMillis;
        lastArrowFrame = (nowMillis / 200) % 8;
        lastBarFrame = (nowMillis / 100) % (MATRIX_WIDTH - 12);
        lastPercent = percent;
        errorShown = failed;
    }
    if (failed)
        firmwareUpdateDisplayActive = false;
}


void FirmwareUpdateReboot()
{
    if (!firmwareUpdateDisplayActive || errorShown || lastPercent != 100)
        return;
    // Success only, after the HTTP response and client close. Keep normal
    // renderers out and yield to WiFi without re-entering application handlers.
    for (uint8_t frame = 0; frame < 16; ++frame)
    {
        DrawProgress(100, false);
        delay(100);
    }
    matrix->clear();
    const uint16_t letters[] = {0x6BAD, 0x79A7, 0x6BAE, 0x7B6F, 0x7B6F, 0x7492};
    const int x = (MATRIX_WIDTH - 23) / 2;
    for (uint8_t letter = 0; letter < 6; ++letter)
        Glyph(x + letter * 4, letters[letter], 0, true);
    matrix->show();
    delay(2000);
}

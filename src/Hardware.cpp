#include "FirmwareState.h"
#include "Tools.h"

void getBatteryVoltage()
{
#if defined(ESP32)
    // Zero means no battery input. GPIO0 belongs to ADC2, shared with WiFi.
    // Keep real battery pins (including Ulanzi's GPIO_NUM_34) unchanged.
    if (VBAT_PIN <= 0) return;
#endif
    uint16_t value = 0;
    uint8_t numReadings = 5;

    for (uint8_t i = 0; i < numReadings; i++)
    {
        value = value + analogRead(VBAT_PIN);

        // 1ms pause adds more stability between reads.
        delay(1);
    }

    batteryLevel = map(value / numReadings, MIN_BATTERY, MAX_BATTERY, 0, 100);
    if (batteryLevel >= 100)
    {
        batteryLevel = 100;
    }
    if (batteryLevel <= 0)
    {
        batteryLevel = 1;
    }
}

void SetCurrentMatrixBrightness(int newBrightness)
{
    currentMatrixBrightness = newBrightness;
    matrix->setBrightness(currentMatrixBrightness);
}

namespace
{
int16_t connectingTextX = 0, connectingTextY = 6;
bool connectingTextPlaced = false;

void DrawConnectingText()
{
    matrix->setFont(&PixelItFont);
    matrix->setTextWrap(false);
    matrix->setTextColor(matrix->Color(255, 255, 255));
    matrix->clear();
    if (!connectingTextPlaced)
    {
        // PixelItFont glyph widths include blank columns. Measure actual ink
        // once using the existing matrix buffer, above the reserved bar row.
        matrix->setCursor(0, 6);
        matrix->print("WiFi");
        int16_t left = MATRIX_WIDTH, right = -1, top = MATRIX_HEIGHT, bottom = -1;
        for (int y = 0; y < MATRIX_HEIGHT - 1; ++y)
            for (int x = 0; x < MATRIX_WIDTH; ++x)
            {
                const CRGB &pixel = leds[matrix->XY(x, y)];
                if (pixel.r || pixel.g || pixel.b)
                {
                    if (x < left) left = x;
                    if (x > right) right = x;
                    if (y < top) top = y;
                    if (y > bottom) bottom = y;
                }
            }
        if (right >= left && bottom >= top)
        {
            connectingTextX = (MATRIX_WIDTH - (right - left + 1)) / 2 - left;
            connectingTextY = 6 + (MATRIX_HEIGHT - 1 - (bottom - top + 1)) / 2 - top;
        }
        connectingTextPlaced = true;
        matrix->clear();
    }
    matrix->setCursor(connectingTextX, connectingTextY);
    matrix->print("WiFi");
}
}

void ConnectingAnimationTick()
{
    if (!wifiScreenAktiv) return;
    // Bar moves every tick
    connectingBarX += connectingBarDir;
    if (connectingBarX >= MATRIX_WIDTH - 1)
    {
        connectingBarX = MATRIX_WIDTH - 1;
        connectingBarDir = -1;
    }
    else if (connectingBarX <= 0)
    {
        connectingBarX = 0;
        connectingBarDir = 1;
    }

    DrawConnectingText();

    // Draw bouncing bar with fading trail on bottom row
    for (uint8_t i = 0; i < CONNECTING_TRAIL_LEN; i++)
    {
        int16_t x = connectingBarX - connectingBarDir * (int8_t)i;
        if (x >= 0 && x < MATRIX_WIDTH)
        {
            matrix->drawPixel(x, MATRIX_HEIGHT - 1, LogoGradientColor(i, CONNECTING_TRAIL_LEN, CONNECTING_TRAIL_BRIGHTNESS[i]));
        }
    }

    matrix->show();
}

void EnteredHotspotCallback(WiFiManager *manager)
{
    connectingAnimTicker.detach();
    Log(F("Hotspot"), "Waiting for WiFi configuration");
    if (!wifiScreenAktiv) return;
    matrix->clear();
    DrawTextHelper("HOTSPOT", false, false, false, false, false, 255, 255, 255, 3, 1);
    FadeIn();
}

void ShowConnectingScreen()
{
    if (!wifiScreenAktiv)
    {
        connectingAnimTicker.detach();
        matrix->clear();
        matrix->show();
        return;
    }
    // Bar starts at left edge, moving right
    connectingBarX = 0;
    connectingBarDir = 1;

    DrawConnectingText();
    matrix->drawPixel(0, MATRIX_HEIGHT - 1,
                      LogoGradientColor(0, CONNECTING_TRAIL_LEN, CONNECTING_TRAIL_BRIGHTNESS[0]));
    FadeIn();

    connectingAnimTicker.attach_ms(60, ConnectingAnimationTick);
}


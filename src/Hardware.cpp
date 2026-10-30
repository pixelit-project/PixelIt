#include "FirmwareState.h"
#include "Tools.h"

void getBatteryVoltage()
{
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

void ConnectingAnimationTick()
{
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

    // Redraw static centered "WiFi" text (cleared each frame)
    matrix->setFont(&PixelItFont);
    matrix->setTextWrap(false);

    int16_t bx, by;
    uint16_t bw, bh;
    matrix->getTextBounds("WiFi", 0, 0, &bx, &by, &bw, &bh);
    int16_t textX = (MATRIX_WIDTH - (int16_t)(bw - 4)) / 2;

    matrix->clear();
    matrix->setTextColor(matrix->Color(255, 255, 255));
    matrix->setCursor(textX, 6);
    matrix->print("WiFi");

    // Draw bouncing bar with fading trail on bottom row
    for (uint8_t i = 0; i < CONNECTING_TRAIL_LEN; i++)
    {
        int16_t x = connectingBarX - connectingBarDir * (int8_t)i;
        if (x >= 0 && x < MATRIX_WIDTH)
        {
            matrix->drawPixel(x, MATRIX_HEIGHT - 1, matrix->Color(0, 0, CONNECTING_TRAIL_BRIGHTNESS[i]));
        }
    }

    matrix->show();
}

void EnteredHotspotCallback(WiFiManager *manager)
{
    connectingAnimTicker.detach();
    Log(F("Hotspot"), "Waiting for WiFi configuration");
    matrix->clear();
    DrawTextHelper("HOTSPOT", false, false, false, false, false, 255, 255, 255, 3, 1);
    FadeIn();
}

void ShowConnectingScreen()
{
    // Bar starts at left edge, moving right
    connectingBarX = 0;
    connectingBarDir = 1;

    // Draw initial frame: centered "WiFi" text + bar head at left
    matrix->setFont(&PixelItFont);
    matrix->setTextWrap(false);
    int16_t bx, by;
    uint16_t bw, bh;
    matrix->getTextBounds("WiFi", 0, 0, &bx, &by, &bw, &bh);
    int16_t textX = (MATRIX_WIDTH - (int16_t)(bw - 4)) / 2;

    matrix->clear();
    matrix->setTextColor(matrix->Color(255, 255, 255));
    matrix->setCursor(textX, 6);
    matrix->print("WiFi");
    matrix->drawPixel(0, MATRIX_HEIGHT - 1, matrix->Color(0, 0, CONNECTING_TRAIL_BRIGHTNESS[0]));
    FadeIn();

    connectingAnimTicker.attach_ms(60, ConnectingAnimationTick);
}


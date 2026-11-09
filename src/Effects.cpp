#include "FirmwareState.h"
#include "Tools.h"

void FadeOut(int dealy, int minBrightness)
{
    int currentFadeBrightness = currentMatrixBrightness;

    int counter = 25;
    while (counter >= 0)
    {
        currentFadeBrightness = map(counter, 0, 25, minBrightness, currentMatrixBrightness);
        matrix->setBrightness(currentFadeBrightness);
        matrix->show();
        counter--;
        delay(dealy);
    }
}

void FadeIn(int dealy, int minBrightness)
{
    int currentFadeBrightness = minBrightness;

    int counter = 0;
    while (counter <= 25)
    {
        currentFadeBrightness = map(counter, 0, 25, minBrightness, currentMatrixBrightness);
        matrix->setBrightness(currentFadeBrightness);
        matrix->show();
        counter++;
        delay(dealy);
    }
}

void ColorFlash(int red, int green, int blue)
{
    for (uint16_t row = 0; row < 8; row++)
    {
        for (uint16_t column = 0; column < 32; column++)
        {
            matrix->drawPixel(column, row, matrix->Color(red, green, blue));
        }
    }
    matrix->show();
}

uint ColorWheel(byte wheelPos, int pos)
{
    if (wheelPos < 85)
    {
        return matrix->Color((wheelPos * 3) - pos, (255 - wheelPos * 3) - pos, 0);
    }
    else if (wheelPos < 170)
    {
        wheelPos -= 85;
        return matrix->Color((255 - wheelPos * 3) - pos, 0, (wheelPos * 3) - pos);
    }
    else
    {
        wheelPos -= 170;
        return matrix->Color(0, (wheelPos * 3) - pos, (255 - wheelPos * 3) - pos);
    }
}

void ColorLogoText(int left, int width, int whiteFrom)
{
    for (int y = 0; y < MATRIX_HEIGHT; ++y)
        for (int x = left; x < left + width && x < MATRIX_WIDTH; ++x)
        {
            if (x < 0) continue;
            const CRGB &pixel = leds[matrix->XY(x, y)];
            if (pixel.r || pixel.g || pixel.b)
                matrix->drawPixel(x, y, x >= whiteFrom ? matrix->Color(255, 255, 255)
                                                      : LogoGradientColor(x - left, width));
        }
}

namespace
{
void ApplyBootLogoColors()
{
    if (bootScreenStyle != 1)
        ColorLogoText((MATRIX_WIDTH / 2) - 12, 23, (MATRIX_WIDTH / 2) + 6);
}
}

void ShowBootAnimation()
{
    DrawTextHelper("P", false, false, false, false, false, 255, 51, 255, (MATRIX_WIDTH / 2) - 12, 1);
    ApplyBootLogoColors();
    matrix->show();

    delay(200);
    DrawTextHelper("I", false, false, false, false, false, 0, 255, 42, (MATRIX_WIDTH / 2) - 8, 1);
    ApplyBootLogoColors();
    matrix->show();

    delay(200);
    DrawTextHelper("X", false, false, false, false, false, 255, 25, 25, (MATRIX_WIDTH / 2) - 6, 1);
    ApplyBootLogoColors();
    matrix->show();

    delay(200);
    DrawTextHelper("E", false, false, false, false, false, 25, 255, 255, (MATRIX_WIDTH / 2) - 2, 1);
    ApplyBootLogoColors();
    matrix->show();

    delay(200);
    DrawTextHelper("L", false, false, false, false, false, 255, 221, 51, (MATRIX_WIDTH / 2) + 2, 1);
    ApplyBootLogoColors();
    matrix->show();

    delay(500);
    DrawTextHelper("I", false, false, false, false, false, 255, 255, 255, (MATRIX_WIDTH / 2) + 6, 1);
    DrawTextHelper("T", false, false, false, false, false, 255, 255, 255, (MATRIX_WIDTH / 2) + 8, 1);
    ApplyBootLogoColors();
    matrix->show();
    delay(1000);
}

void ShowBatteryScreen()
{
    const size_t capacity = JSON_ARRAY_SIZE(64) + JSON_OBJECT_SIZE(1) + 2 * JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + 350;
    DynamicJsonBuffer jsonBuffer(capacity);
    const char *json = "{\"bitmap\":{\"data\":[0,0,65535,65535,65535,0,0,0,0,0,65535,2016,65535,0,0,0,0,65535,2016,2016,2016,65535,0,0,0,65535,2016,2016,2016,65535,0,0,0,65535,2016,2016,2016,65535,0,0,0,65535,2016,2016,2016,65535,0,0,0,65535,2016,2016,2016,65535,0,0,0,65535,65535,65535,65535,65535,0,0],\"position\":{\"x\":0,\"y\":0},\"size\":{\"width\":8,\"height\":8}}}";
    JsonObject &root = jsonBuffer.parseObject(json);
    if (root.success())
    {
        Serial.println("parsed json");
    }
    else
    {
        Serial.println("failed to parse json");
    }
    getBatteryVoltage();
    matrix->clear();
    DrawSingleBitmap(root["bitmap"]);
    DrawTextHelper(String(batteryLevel, 0) + "%", false, true, false, false, false, 255, 255, 255, 9, 1);
    matrix->show();
    delay(1000);
}

ColorTemperature GetUserColorTemp()
{
    if (matrixTempCorrection == "tungsten40w")
    {
        return Tungsten40W;
    }

    if (matrixTempCorrection == "tungsten100w")
    {
        return Tungsten100W;
    }

    if (matrixTempCorrection == "halogen")
    {
        return Halogen;
    }

    if (matrixTempCorrection == "carbonarc")
    {
        return CarbonArc;
    }

    if (matrixTempCorrection == "highnoonsun")
    {
        return HighNoonSun;
    }

    if (matrixTempCorrection == "directsunlight")
    {
        return DirectSunlight;
    }

    if (matrixTempCorrection == "overcastsky")
    {
        return OvercastSky;
    }

    if (matrixTempCorrection == "clearbluesky")
    {
        return ClearBlueSky;
    }

    if (matrixTempCorrection == "warmfluorescent")
    {
        return WarmFluorescent;
    }

    if (matrixTempCorrection == "standardfluorescent")
    {
        return StandardFluorescent;
    }

    if (matrixTempCorrection == "coolwhitefluorescent")
    {
        return CoolWhiteFluorescent;
    }
    if (matrixTempCorrection == "fullspectrumfluorescent")
    {
        return FullSpectrumFluorescent;
    }
    if (matrixTempCorrection == "growlightfluorescent")
    {
        return GrowLightFluorescent;
    }
    if (matrixTempCorrection == "blacklightfluorescent")
    {
        return BlackLightFluorescent;
    }
    if (matrixTempCorrection == "mercuryvapor")
    {
        return MercuryVapor;
    }
    if (matrixTempCorrection == "sodiumvapor")
    {
        return SodiumVapor;
    }
    if (matrixTempCorrection == "metalhalide")
    {
        return MetalHalide;
    }
    if (matrixTempCorrection == "highpressuresodium")
    {
        return HighPressureSodium;
    }

    return UncorrectedTemperature;
}

LEDColorCorrection GetUserColorCorrection()
{
    if (matrixTempCorrection == "default")
    {
        return TypicalSMD5050;
    }

    if (matrixTempCorrection == "typicalsmd5050")
    {
        return TypicalSMD5050;
    }

    if (matrixTempCorrection == "typical8mmpixel")
    {
        return Typical8mmPixel;
    }

    return UncorrectedColor;
}


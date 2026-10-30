#include "FirmwareState.h"
#include "Tools.h"

int *GetUserCutomCorrection()
{
    String rgbString = matrixTempCorrection;
    rgbString.trim();

    // R,G,B / 255,255,255
    static int rgbArray[3];

    // R
    rgbArray[0] = rgbString.substring(0, 3).toInt();
    // G
    rgbArray[1] = rgbString.substring(4, 7).toInt();
    // B
    rgbArray[2] = rgbString.substring(8, 11).toInt();

    return rgbArray;
}

LightDependentResistor::ePhotoCellKind TranslatePhotocell(String photocell)
{
    if (photocell == "GL5516")
        return LightDependentResistor::GL5516;
    if (photocell == "GL5528")
        return LightDependentResistor::GL5528;
    if (photocell == "GL5537_1")
        return LightDependentResistor::GL5537_1;
    if (photocell == "GL5537_2")
        return LightDependentResistor::GL5537_2;
    if (photocell == "GL5539")
        return LightDependentResistor::GL5539;
    if (photocell == "GL5549")
        return LightDependentResistor::GL5549;
    Log(F("LDR assignment - unknown type"), photocell);
    return LightDependentResistor::GL5528;
}

uint8_t TranslatePin(String pin)
{
#if defined(ESP8266)
    if (pin == "Pin_D0")
        return D0;
    if (pin == "Pin_D1")
        return D1;
    if (pin == "Pin_D2")
        return D2;
    if (pin == "Pin_D3")
        return D3;
    if (pin == "Pin_D4")
        return D4;
    if (pin == "Pin_D5")
        return D5;
    if (pin == "Pin_D6")
        return D6;
    if (pin == "Pin_D7")
        return D7;
    if (pin == "Pin_D8")
        return D8;
    if (pin == "Pin_27")
        return 27;
    Log(F("Pin assignment - unknown pin"), pin);
    return LED_BUILTIN;
#elif defined(ESP32)

    if (pin == "GPIO_NUM_14")
        return (int)GPIO_NUM_14;
    if (pin == "GPIO_NUM_15")
        return (int)GPIO_NUM_15;
    if (pin == "GPIO_NUM_16")
        return (int)GPIO_NUM_16;
    if (pin == "GPIO_NUM_17")
        return (int)GPIO_NUM_17;
    if (pin == "GPIO_NUM_18")
        return (int)GPIO_NUM_18;
    if (pin == "GPIO_NUM_19")
        return (int)GPIO_NUM_19;
    if (pin == "GPIO_NUM_21")
        return (int)GPIO_NUM_21;
    if (pin == "GPIO_NUM_22")
        return (int)GPIO_NUM_22;
    if (pin == "GPIO_NUM_23")
        return (int)GPIO_NUM_23;
    if (pin == "GPIO_NUM_25")
        return (int)GPIO_NUM_25;
    if (pin == "GPIO_NUM_26")
        return (int)GPIO_NUM_26;
    if (pin == "GPIO_NUM_27")
        return (int)GPIO_NUM_27;
    if (pin == "SPI_CLK_GPIO_NUM")
        return (int)SPI_CLK_GPIO_NUM;
    if (pin == "SPI_CS0_GPIO_NUM")
        return (int)SPI_CS0_GPIO_NUM;

    Log(F("Pin assignment - unknown pin"), pin);
    return (int)GPIO_NUM_32; // IDK
#endif
}

void ClearTextArea()
{
    int16_t h = 8;
    int16_t w = 24;
    int16_t x = 8;
    int16_t y = 0;

    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            matrix->drawPixel(x + i, y, (uint16_t)0);
        }
    }
}

void ClearBMPArea()
{
    int16_t h = bmpHeight;
    int16_t w = bmpWidth;
    int16_t x = bmpPosX;
    int16_t y = bmpPosY;

    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            matrix->drawPixel(x + i, y, (uint16_t)0);
        }
    }
}

int DayOfWeekFirstMonday(int OrigDayofWeek)
{
    int idx = (7 + OrigDayofWeek) - 1;
    if (idx > 6) // week ends at 6, because Enum.DayOfWeek is zero-based
    {
        idx -= 7;
    }
    return idx;
    // int diff = (7 + (OrigDayofWeek - 1)) % 7;
    // return OrigDayofWeek + (-1 * diff);
}

void initDFPlayer()
{
    if (!mp3Player.begin(*softSerial))
    {
        Log(F("DFPlayer"), F("DFPlayer not found"));
    }
    else
    {
        Log(F("DFPlayer"), F("DFPlayer started"));
        mp3Player.volume(initialVolume);
    }
}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////

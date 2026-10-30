#include "FirmwareState.h"
#include "Tools.h"

void DrawText(String text, int bigFont, int colorRed, int colorGreen, int colorBlue, int posX, int posY)
{
    DrawTextHelper(text, bigFont, false, false, false, false, colorRed, colorGreen, colorBlue, posX, posY);
}

void DrawTextCenter(String text, int bigFont, int colorRed, int colorGreen, int colorBlue, int posX, int posY)
{
    DrawTextHelper(text, bigFont, true, false, false, false, colorRed, colorGreen, colorBlue, posX, posY);
}

void DrawTextScrolled(String text, int bigFont, bool centerText, bool fadeInRequired, int colorRed, int colorGreen, int colorBlue, int posX, int posY)
{
    DrawTextHelper(text, bigFont, centerText, true, false, fadeInRequired, colorRed, colorGreen, colorBlue, posX, posY);
}

void DrawAutoTextScrolled(String text, int bigFont, bool centerText, bool fadeInRequired, int colorRed, int colorGreen, int colorBlue, int posX, int posY)
{
    DrawTextHelper(text, bigFont, centerText, false, true, fadeInRequired, colorRed, colorGreen, colorBlue, posX, posY);
}

void DrawTextHelper(String text, int bigFont, bool centerText, bool scrollText, bool autoScrollText, bool fadeInRequired, int colorRed, int colorGreen, int colorBlue, int posX, int posY)
{
    uint16_t xTextWidth, xAvailableTextSpace;
    int16_t boundsx1, boundsy1;
    uint16_t boundsw, boundsh;

    text = Utf8ToAscii(text);

    // Debug function values
    // Serial.printf("DrawTextHelper: %s (bigFont: %s, centerText: %s, scrollText: %s, autoScrollText: %s: %s, fadeInRequired: %s)\n", text.c_str(), bigFont == 1 ? "true" : "false", centerText ? "true" : "false", scrollText ? "true" : "false", autoScrollText ? "true" : "false" ? "true" : "false", fadeInRequired ? "true" : "false");

    // Available text space depends on matrix size
    xAvailableTextSpace = MATRIX_WIDTH - posX;

    if (bigFont == 1)
    {
        // Set large font
        matrix->setFont();
        matrix->getTextBounds(text, 0, 0, &boundsx1, &boundsy1, &boundsw, &boundsh);
        xTextWidth = boundsw;

        // Position correction
        posY = posY - 1;
    }
    else if (bigFont == 2) // fat font, only to be used for time display
    {
        // Set fat font
        matrix->setFont(&FatPixels);

        matrix->getTextBounds(text, 0, 0, &boundsx1, &boundsy1, &boundsw, &boundsh);
        xTextWidth = boundsw;

        // Position correction
        posY = posY + 6;
    }
    else if (bigFont == 3) // very large font, only to be used for time display
    {
        // Set very large font
        matrix->setFont(&LargePixels);

        matrix->getTextBounds(text, 0, 0, &boundsx1, &boundsy1, &boundsw, &boundsh);
        xTextWidth = boundsw;

        // Position correction
        posY = posY + 6;
    }
    else
    {
        // Set small font
        matrix->setFont(&PixelItFont);
        matrix->getTextBounds(text, 0, 0, &boundsx1, &boundsy1, &boundsw, &boundsh);
        xTextWidth = boundsw - 4;

        // Position correction
        posY = posY + 5;
    }

    if (centerText && xTextWidth < xAvailableTextSpace) // center text if text is smaller than available pixels
    {
        // Calculation for the first pixel of the text
        posX = posX + ((xAvailableTextSpace - xTextWidth) / 2);
    }

    matrix->setCursor(posX, posY);

    matrix->setTextColor(matrix->Color(colorRed, colorGreen, colorBlue));

    if (scrollText || (autoScrollText && xTextWidth > xAvailableTextSpace)) // scroll text if text is larger than available pixels
    {

        matrix->setBrightness(currentMatrixBrightness);

        scrollTextString = text;
        scrollposY = posY;
        scrollposX = posX;
        scrollxTextWidth = xTextWidth;
        scrollxAvailableTextSpace = xAvailableTextSpace;
        scrollCurPos = MATRIX_WIDTH + 1;

        scrollTextAktivLoop = true;
        scrollTextPrevMillis = millis();
        ScrollText(fadeInRequired);
    }
    // In case the text on the display fits!
    else if (autoScrollText)
    {
        matrix->print(text);
        // Here you have to check if the fade out was done,
        // then the brightness is too low (0),
        // but only if animateBMPAktivLoop is not active,
        // because otherwise it's already done.
        if (fadeInRequired && !animateBMPAktivLoop)
        {
            FadeIn();
        }
        else
        {
            matrix->show();
        }
    }
    else
    {
        matrix->print(text);
    }
}

void ScrollText(bool isFadeInRequired)
{
    int xOffset = MATRIX_WIDTH - scrollxAvailableTextSpace;

    if (scrollCurPos > ((scrollxTextWidth - xOffset) * -1))
    {
        matrix->clear();
        matrix->setCursor(--scrollCurPos, scrollposY);
        matrix->print(scrollTextString);

        // draw black pixel under icon / blank space if (xOffset > 0)
        for (int i = 0; i < xOffset; i++)
        {
            matrix->drawLine(i, 0, i, MATRIX_HEIGHT, matrix->Color(0, 0, 0));
        }

        if (withBMP && bmpArray)
        {
            matrix->drawRGBBitmap(bmpPosX, bmpPosY, bmpArray.get(), bmpWidth, bmpHeight);
        }

        if (isFadeInRequired)
        {
            FadeIn();
        }
        else
        {
            matrix->show();
        }
    }
    else
    {
        scrollCurPos = MATRIX_WIDTH;
    }
}

void AnimateBMP(bool isShowRequired)
{
    if (animationBmpCount == 0 || !animationBmpList || !bmpArray)
    {
        animateBMPAktivLoop = false;
        return;
    }

    if (animateBMPCounter >= static_cast<int>(animationBmpCount))
    {
        // Ist ein Repeat Limit übergeben worden.
        if (animateBMPLimitLoops > 0 && (!animateBMPRubberbandingAktiv || animationBmpCount == 1))
        {
            animateBMPLoopCount++;

            // Ist der Repeat Limit erreicht den AnimateBMP Loop deaktiveren.
            if (animateBMPLoopCount == animateBMPLimitLoops)
            {
                animateBMPAktivLoop = false;
                return;
            }
        }

        // Prüfen ob Rubberbanding aktiv und mehr wie 1 Frame vorhanden ist.
        if (animateBMPRubberbandingAktiv && animateBMPCounter > 1)
        {
            animateBMPReverse = true;

            // 2 abziehen da sonst der letzte Frame doppelt angezeigt wird.
            if (animateBMPCounter > 0)
            {
                animateBMPCounter = animateBMPCounter - 2;
            }
        }
        else
        {
            animateBMPCounter = 0;
        }
    }

    if (animateBMPCounter < 0)
    {
        // Auf 1 setzten da sons der erste Frame doppelt angezeigt wird.
        animateBMPCounter = 1;
        animateBMPReverse = false;

        // Ist ein Repeat Limit übergeben worden.
        if (animateBMPLimitLoops > 0)
        {
            animateBMPLoopCount++;
            // Ist der Repeat Limit erreicht den AnimateBMP Loop deaktiveren.
            if (animateBMPLoopCount >= animateBMPLimitLoops)
            {
                animateBMPAktivLoop = false;
                return;
            }
        }
    }

    ClearBMPArea();

    const unsigned long frameMillis = millis();
    if (animationRenderedFrames > 0 && frameMillis - animationLastRenderedMillis > animationMaxFrameGap)
        animationMaxFrameGap = frameMillis - animationLastRenderedMillis;
    animationLastRenderedMillis = frameMillis;
    animationRenderedFrames++;

    matrix->drawRGBBitmap(bmpPosX, bmpPosY, animationBmpList.get() + animateBMPCounter * animationBmpPixels, bmpWidth, bmpHeight);

    memcpy(bmpArray.get(), animationBmpList.get() + animateBMPCounter * animationBmpPixels, animationBmpPixels * sizeof(uint16_t));

    // Soll der Loop wieder zurücklaufen?
    if (animateBMPReverse)
    {
        animateBMPCounter--;
    }
    else
    {
        animateBMPCounter++;
    }

    if (isShowRequired)
    {
#if defined(ESP8266)
        const uint32_t sentBefore = _frame_cnt;
#endif
        matrix->show();
#if defined(ESP8266)
        if (_frame_cnt == sentBefore)
            animationFailedLedFrames++;
#endif
    }
}

void DrawSingleBitmap(JsonObject &json)
{
    int16_t h = json["size"]["height"].as<int16_t>();
    int16_t w = json["size"]["width"].as<int16_t>();
    int16_t x = json["position"]["x"].as<int16_t>();
    int16_t y = json["position"]["y"].as<int16_t>();

    if (w <= 0 || h <= 0 || w > MATRIX_WIDTH || h > MATRIX_HEIGHT ||
        !json["data"].is<JsonArray>() || json["data"].as<JsonArray>().size() != static_cast<size_t>(w * h))
    {
        Log(F("Bitmap"), F("Invalid dimensions or pixel count"));
        return;
    }
    if (!PrepareBitmapStorage())
    {
        Log(F("Bitmap"), F("Insufficient snapshot memory"));
        return;
    }
    bmpHeight = h;
    bmpWidth = w;
    bmpPosX = x;
    bmpPosY = y;
    withBMP = true;

    // Hier kann leider nicht die Funktion matrix->drawRGBBitmap() genutzt werde da diese Fehler in der Anzeige macht wenn es mehr wie 8x8 Pixel werden.
    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            matrix->drawPixel(x + i, y, json["data"][j * w + i].as<uint16_t>());
        }
    }

    // JsonArray in IntArray konvertieren
    // dies ist nötig für diverse kleine Logiken z.B. Scrolltext
    // bei Multibitmaps landet hier nur eine der Bitmaps - das ist aber egal, da dann eh nicht gescrollt wird
    json["data"].as<JsonArray>().copyTo(bmpArray.get(), MAX_BITMAP_PIXELS);
}

void DrawClock(bool fromJSON)
{
    matrix->clear();

    char date[14];
    char time[14];

    int xPosTime = 0;

    // use Adafruit 7px font as large clock font by default
    uint clockFontChoice = 1;
    if (clockFatFont) // use fat 8px clock font if set explicitly
    {
        clockFontChoice = 2;
    }
    else if (clockLargeFont) // use large 8px clock font if set explicitly
    {
        clockFontChoice = 3;
    }

    // check if a large font is set
    bool clockFontIsLarge = false;
    if (clockLargeFont || clockFatFont)
    {
        clockFontIsLarge = true;
    }

    if (clockDateDayMonth)
    {
        sprintf_P(date, PSTR("%02d.%02d."), day(), month());
    }
    else
    {
        sprintf_P(date, PSTR("%02d/%02d"), month(), day());
    }

    if (clock24Hours && clockWithSeconds && !clockFontIsLarge)
    {
        xPosTime = 2;
        sprintf_P(time, PSTR("%02d:%02d:%02d"), hour(), minute(), second());
    }
    else if (!clock24Hours)
    {
        xPosTime = 2;

        if (clockBlink && clockBlinkAnimated)
        {
            clockBlink = false;
            if (!clockFontIsLarge)
            {
                sprintf_P(time, PSTR("%2d %02d %s"), hourFormat12(), minute(), isAM() ? "AM" : "PM");
            }
            else
            {
                sprintf_P(time, PSTR("%2d %02d"), hourFormat12(), minute());
            }
        }
        else
        {
            clockBlink = !clockBlink;
            if (!clockFontIsLarge)
            {
                sprintf_P(time, PSTR("%2d:%02d %s"), hourFormat12(), minute(), isAM() ? "AM" : "PM");
            }
            else
            {
                sprintf_P(time, PSTR("%2d:%02d"), hourFormat12(), minute());
            }
        }
    }
    else
    {
        xPosTime = 7;

        if (clockBlink && clockBlinkAnimated)
        {
            clockBlink = false;
            sprintf_P(time, PSTR("%02d %02d"), hour(), minute());
        }
        else
        {
            clockBlink = !clockBlink;
            sprintf_P(time, PSTR("%02d:%02d"), hour(), minute());
        }
    }

    if (!clockSwitchAktiv || (clockSwitchAktiv && clockCounterClock <= clockSwitchSec))
    {
        if (clockSwitchAktiv)
        {
            clockCounterClock++;
        }
        if (clockCounterClock > clockSwitchSec)
        {
            clockCounterDate = 0;

            if (clockFontIsLarge) // fade rather than vertical animate purely because DrawTextCenter doesnt have a Y argument...
            {
                DrawTextCenter(String(time), clockFontChoice, clockColorR, clockColorG, clockColorB, 0, 1);
                FadeOut(30);
                matrix->clear();
                DrawTextCenter(String(date), clockFontChoice, clockColorR, clockColorG, clockColorB, 0, 1);
                FadeIn(30);
            }
            else
            {
                int counter = 0;
                while (counter <= 6) // vertical animate
                {
                    counter++;
                    matrix->clear();
                    DrawText(String(time), false, clockColorR, clockColorG, clockColorB, xPosTime, (1 + counter));
                    DrawText(String(date), false, clockColorR, clockColorG, clockColorB, 7, (-6 + counter));
                    matrix->drawLine(0, 7, 33, 7, 0);
                    if (clockDrawWeekDays)
                    {
                        DrawWeekDay();
                    }
                    matrix->show();
                    delay(35);
                }
            }
        }
        else if (clockFontIsLarge)
        {

            DrawTextCenter(String(time), clockFontChoice, clockColorR, clockColorG, clockColorB, 0, 1);
        }
        else
        {
            DrawText(String(time), false, clockColorR, clockColorG, clockColorB, xPosTime, 1);
            xPosTime = 3;
        }
    }
    else
    {
        clockCounterDate++;

        if (clockCounterDate == clockSwitchSec)
        {
            clockCounterClock = 0;

            if (clockFontIsLarge) // fade rather than vertical animate purely because DrawTextCenter doesnt have a Y argument...
            {
                DrawTextCenter(String(date), 2, clockColorR, clockColorG, clockColorB, 0, 1);
                FadeOut(30);
                matrix->clear();
                DrawTextCenter(String(time), 2, clockColorR, clockColorG, clockColorB, 0, 1);
                FadeIn(30);
            }
            else
            {
                int counter = 0;
                while (counter <= 6) // vertical animate
                {
                    counter++;
                    matrix->clear();
                    DrawText(String(date), false, clockColorR, clockColorG, clockColorB, 7, (1 + counter));
                    DrawText(String(time), false, clockColorR, clockColorG, clockColorB, xPosTime, (-6 + counter));
                    matrix->drawLine(0, 7, 33, 7, 0);
                    if (clockDrawWeekDays)
                    {
                        DrawWeekDay();
                    }
                    matrix->show();
                    delay(35);
                }
            }
        }
        else if (clockFontIsLarge)
        {
            DrawTextCenter(String(date), clockFontChoice, clockColorR, clockColorG, clockColorB, 0, 1);
        }
        else
        {
            DrawText(String(date), false, clockColorR, clockColorG, clockColorB, 7, 1);
        }
    }

    if (!clockFontIsLarge && clockDrawWeekDays)
    {
        DrawWeekDay();
    }

    // Wenn der Aufruf nicht über JSON sondern über den Loop kommt
    // muss ich mich selbst ums Show kümmern.
    if (!fromJSON)
    {
        matrix->show();
    }
}

void DrawWeekDay()
{
    // The Libary works with dayOfWeek with Sunday = 1...
    // So Sunday = 1 <-> Saturday = 7
    int weekDayNumber = 0;
    if (clockDayOfWeekFirstMonday)
    {
        weekDayNumber = DayOfWeekFirstMonday(dayOfWeek(now()) - 1);
    }
    else
    {
        weekDayNumber = dayOfWeek(now()) - 1;
    }

    for (int i = 0; i <= 6; i++)
    {
        if (i == weekDayNumber)
        {
            matrix->drawLine(2 + i * 4, 7, i * 4 + 4, 7, matrix->Color(clockColorR, clockColorG, clockColorB));
        }
        else
        {
            matrix->drawLine(2 + i * 4, 7, i * 4 + 4, 7, 21162);
        }
    }
}


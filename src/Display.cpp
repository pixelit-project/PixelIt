#include "FirmwareState.h"
#include "Tools.h"

void DrawText(String text, int bigFont, int colorRed, int colorGreen, int colorBlue, int posX, int posY)
{
    DrawTextHelper(text, bigFont, false, false, false, false, colorRed, colorGreen, colorBlue, posX, posY);
}

namespace
{
constexpr unsigned maxCharacterColors = 64;

struct TextColors
{
    CRGB base;
    CRGB gradientEnd;
    bool gradient = false;
    bool rainbow = false;
    uint16_t overrides[maxCharacterColors];
    uint64_t overrideMask = 0;
    int8_t xAxis = 0, yAxis = 0;
    int16_t boundsX = 0, boundsY = 0;
    uint16_t boundsWidth = 0, boundsHeight = 0;
    uint16_t cycleMs = 0;

    bool active() const { return gradient || rainbow || overrideMask; }
};

TextColors scrollColors;

bool ParseTextHex(const char *hex, CRGB &color)
{
    if (!hex || *hex++ != '#')
        return false;

    uint32_t rgb = 0;
    for (unsigned i = 0; i < 6; ++i)
    {
        const char digit = hex[i];
        unsigned value;
        if (digit >= '0' && digit <= '9')
            value = digit - '0';
        else if (digit >= 'a' && digit <= 'f')
            value = digit - 'a' + 10;
        else if (digit >= 'A' && digit <= 'F')
            value = digit - 'A' + 10;
        else
            return false;
        rgb = (rgb << 4) | value;
    }
    if (hex[6] != '\0')
        return false;

    color = CRGB(rgb);
    return true;
}

void ReadTextColors(TextColors &colors, const JsonObject &options, unsigned length)
{
    const JsonVariant rainbow = options["rainbow"];
    colors.rainbow = rainbow.is<bool>() && rainbow.as<bool>();
    colors.gradient = ParseTextHex(options["gradientEnd"].as<const char *>(), colors.gradientEnd);
    const char *direction = options["gradientDirection"].as<const char *>();
    if (direction)
    {
        static const struct
        {
            const char *name;
            int8_t x, y;
        } directions[] = {
            {"leftToRight", 1, 0}, {"rightToLeft", -1, 0},
            {"topToBottom", 0, 1}, {"bottomToTop", 0, -1},
            {"topLeftToBottomRight", 1, 1}, {"bottomRightToTopLeft", -1, -1},
            {"topRightToBottomLeft", -1, 1}, {"bottomLeftToTopRight", 1, -1}
        };
        for (const auto &entry : directions)
        {
            if (strcmp(direction, entry.name) == 0)
            {
                colors.xAxis = entry.x;
                colors.yAxis = entry.y;
                break;
            }
        }
    }
    const JsonVariant cycle = options["gradientCycleMs"];
    if ((colors.gradient || colors.rainbow) && !cycle.is<bool>() && cycle.is<unsigned long>())
    {
        const unsigned long duration = cycle.as<unsigned long>();
        if (duration >= 250 && duration <= 60000)
        {
            colors.cycleMs = duration;
            if (!colors.xAxis && !colors.yAxis)
                colors.xAxis = 1;
        }
    }
    const JsonArray &overrides = options["characterColors"].as<JsonArray>();
    unsigned index = 0;
    for (JsonVariant value : overrides)
    {
        if (index >= maxCharacterColors || index >= length)
            break;
        CRGB color;
        if (ParseTextHex(value.as<const char *>(), color))
        {
            colors.overrides[index] = matrix->Color(color.r, color.g, color.b);
            colors.overrideMask |= uint64_t{1} << index;
        }
        ++index;
    }
}

uint8_t MovingColorAmount(uint8_t position, uint8_t phase)
{
    const uint8_t shifted = position - phase;
    return shifted < 128 ? shifted * 2 : (255 - shifted) * 2;
}

void PrintDirectionalText(const String &text, const TextColors &colors, uint8_t phase)
{
    // Project the current text bounds and the visible matrix onto the same axis.
    const int x = matrix->getCursorX() + colors.boundsX;
    const int y = matrix->getCursorY() + colors.boundsY;
    const int textWidth = colors.boundsWidth - 1;
    const int textHeight = colors.boundsHeight - 1;
    const int screenWidth = matrix->width() - 1;
    const int screenHeight = matrix->height() - 1;
    const int screenStart = (colors.xAxis < 0 ? -screenWidth : 0) +
                            (colors.yAxis < 0 ? -screenHeight : 0);
    const int textStart = (x + (colors.xAxis < 0 ? textWidth : 0)) * colors.xAxis +
                          (y + (colors.yAxis < 0 ? textHeight : 0)) * colors.yAxis;
    const int span = (colors.xAxis ? textWidth : 0) + (colors.yAxis ? textHeight : 0);
    const int screenSpan = (colors.xAxis ? screenWidth : 0) +
                           (colors.yAxis ? screenHeight : 0);
    uint16_t palette[MATRIX_WIDTH + MATRIX_HEIGHT - 1];
    for (int i = 0; i <= screenSpan; ++i)
    {
        const int distance = screenStart + i - textStart;
        const int clamped = distance < 0 ? 0 : (distance > span ? span : distance);
        CRGB color = colors.base;
        if (colors.rainbow)
            hsv2rgb_rainbow(CHSV(static_cast<uint8_t>(256L * clamped / (span + 1) - phase), 255, 255), color);
        else if (span || colors.cycleMs)
        {
            const uint8_t position = span ? 255L * clamped / (span + (colors.cycleMs != 0)) : 0;
            const uint8_t amount = colors.cycleMs ? MovingColorAmount(position, phase) : position;
            color = blend(colors.base, colors.gradientEnd, amount);
        }
        palette[i] = matrix->Color(color.r, color.g, color.b);
    }

    for (unsigned i = 0; i < text.length(); ++i)
    {
        const bool overridden = i < maxCharacterColors && (colors.overrideMask & (uint64_t{1} << i));
        matrix->setGlyphPalette(overridden ? nullptr : palette, colors.xAxis, colors.yAxis, -screenStart);
        if (overridden)
            matrix->setTextColor(colors.overrides[i]);

        matrix->write(static_cast<uint8_t>(text[i]));
    }
    matrix->setGlyphPalette(nullptr);
    matrix->setTextColor(matrix->Color(colors.base.r, colors.base.g, colors.base.b));
}

void PrintText(const String &text, const TextColors &colors, uint8_t phase = 0)
{
    if (!colors.active())
    {
        matrix->print(text);
        return;
    }

    if ((colors.gradient || colors.rainbow) && (colors.xAxis || colors.yAxis) &&
        colors.boundsWidth && colors.boundsHeight)
    {
        PrintDirectionalText(text, colors, phase);
        return;
    }

    const unsigned length = text.length();
    const uint32_t step = colors.rainbow
                              ? (256UL << 16) / (length ? length : 1)
                              : (length > 1 ? (255UL << 16) / (length - 1) : 0);
    uint32_t position = 0;
    for (unsigned i = 0; i < length; ++i, position += step)
    {
        uint16_t rgb565;
        if (i < maxCharacterColors && (colors.overrideMask & (uint64_t{1} << i)))
        {
            rgb565 = colors.overrides[i];
        }
        else
        {
            CRGB color = colors.base;
            if (colors.rainbow)
                hsv2rgb_rainbow(CHSV(position >> 16, 255, 255), color);
            else if (colors.gradient)
            {
                const uint8_t amount = length > 1 && i == length - 1 ? 255 : position >> 16;
                color = blend(colors.base, colors.gradientEnd, amount);
            }

            rgb565 = matrix->Color(color.r, color.g, color.b);
        }

        matrix->setTextColor(rgb565);
        matrix->write(static_cast<uint8_t>(text[i]));
    }
    matrix->setTextColor(matrix->Color(colors.base.r, colors.base.g, colors.base.b));
}
} // namespace

namespace
{
constexpr uint32_t colorFrameIntervalMs = 60;
uint32_t colorPhaseStart, colorFrameLast;
int16_t animatedTextX, animatedTextY;
const GFXfont *animatedTextFont;
bool animatedTextVisible;
uint8_t colorPhase;

void DrawScrollingTextFrame(int x, bool fadeIn);

void UpdateTextColorPhase(uint32_t now)
{
    if (!scrollColors.cycleMs)
        return;
    const uint32_t elapsed = now - colorPhaseStart;
    const uint32_t position = elapsed % scrollColors.cycleMs;
    colorPhaseStart = now - position;
    colorPhase = (position * 256) / scrollColors.cycleMs;
    colorFrameLast = now;
}

bool RedrawAnimatedText(uint32_t now)
{
    if (!scrollColors.cycleMs || !animatedTextVisible)
        return false;
    UpdateTextColorPhase(now);
    matrix->setFont(animatedTextFont);
    matrix->setCursor(animatedTextX, animatedTextY);
    PrintText(scrollTextString, scrollColors, colorPhase);
    return true;
}
} // namespace

void DrawTextHelper(String text, int bigFont, bool centerText, bool scrollText, bool autoScrollText, bool fadeInRequired, int colorRed, int colorGreen, int colorBlue, int posX, int posY, const JsonObject *options)
{
    uint16_t xTextWidth, xAvailableTextSpace;
    int16_t boundsx1, boundsy1;
    uint16_t boundsw, boundsh;

    text = Utf8ToAscii(text);

    // Debug function values
    // Serial.printf("DrawTextHelper: %s (bigFont: %s, centerText: %s, scrollText: %s, autoScrollText: %s: %s, fadeInRequired: %s)\n", text.c_str(), bigFont == 1 ? "true" : "false", centerText ? "true" : "false", scrollText ? "true" : "false", autoScrollText ? "true" : "false" ? "true" : "false", fadeInRequired ? "true" : "false");

    // Available text space depends on matrix size
    xAvailableTextSpace = MATRIX_WIDTH - posX;

    const GFXfont *font = nullptr;
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
        font = &FatPixels;
        matrix->setFont(font);

        matrix->getTextBounds(text, 0, 0, &boundsx1, &boundsy1, &boundsw, &boundsh);
        xTextWidth = boundsw;

        // Position correction
        posY = posY + 6;
    }
    else if (bigFont == 3) // very large font, only to be used for time display
    {
        // Set very large font
        font = &LargePixels;
        matrix->setFont(font);

        matrix->getTextBounds(text, 0, 0, &boundsx1, &boundsy1, &boundsw, &boundsh);
        xTextWidth = boundsw;

        // Position correction
        posY = posY + 6;
    }
    else
    {
        // Set small font
        font = &PixelItFont;
        matrix->setFont(font);
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

    TextColors colors{};
    colors.base = CRGB(colorRed, colorGreen, colorBlue);
    if (options)
    {
        ReadTextColors(colors, *options, text.length());
        colors.boundsX = boundsx1;
        colors.boundsY = boundsy1;
        colors.boundsWidth = boundsw;
        colors.boundsHeight = boundsh;
    }

    matrix->setTextColor(matrix->Color(colorRed, colorGreen, colorBlue));

    const bool willScroll = scrollText || (autoScrollText && xTextWidth > xAvailableTextSpace);
    if (colors.cycleMs && colors.boundsWidth && colors.boundsHeight)
    {
        animatedTextX = posX;
        animatedTextY = posY;
        animatedTextFont = font;
        animatedTextVisible = !willScroll;
        colorPhaseStart = colorFrameLast = millis();
        colorPhase = 0;
        if (!willScroll)
        {
            scrollTextString = text;
            scrollColors = colors;
        }
    }
    else
    {
        colors.cycleMs = 0;
    }

    if (willScroll)
    {

        matrix->setBrightness(currentMatrixBrightness);

        scrollTextString = text;
        scrollColors = colors;
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
        PrintText(text, colors);
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
            ShowScreenContentFrame();
        }
    }
    else
    {
        PrintText(text, colors);
    }
}

void StopTextColorAnimation()
{
    scrollColors.cycleMs = 0;
    animatedTextVisible = false;
}

bool ServiceTextColorAnimation(uint32_t now)
{
    // Bitmap playback owns its frame budget and paints the overlay on its own frames.
    if (animateBMPAktivLoop || !scrollColors.cycleMs || now - colorFrameLast < colorFrameIntervalMs)
        return false;

    if (scrollTextAktivLoop && animatedTextVisible)
    {
        // Repaint the current scrolling frame, without advancing the text.
        DrawScrollingTextFrame(animatedTextX, false);
        return true;
    }

    if (RedrawAnimatedText(now))
    {
        ShowScreenContentFrame();
        return true;
    }
    return false;
}

namespace
{
void DrawScrollingTextFrame(int x, bool fadeIn)
{
    const int xOffset = MATRIX_WIDTH - scrollxAvailableTextSpace;

    matrix->clear();
    if (scrollColors.cycleMs)
    {
        animatedTextVisible = true;
        animatedTextX = x;
        RedrawAnimatedText(millis());
    }
    else
    {
        matrix->setCursor(x, scrollposY);
        PrintText(scrollTextString, scrollColors);
    }

    for (int i = 0; i < xOffset; i++)
        matrix->drawLine(i, 0, i, MATRIX_HEIGHT, matrix->Color(0, 0, 0));

    if (withBMP && bmpArray)
        matrix->drawRGBBitmap(bmpPosX, bmpPosY, bmpArray.get(), bmpWidth, bmpHeight);

    if (fadeIn)
        FadeIn();
    else
        ShowScreenContentFrame();
}

} // namespace

void ScrollText(bool isFadeInRequired)
{
    const int xOffset = MATRIX_WIDTH - scrollxAvailableTextSpace;
    if (scrollCurPos > ((scrollxTextWidth - xOffset) * -1))
        DrawScrollingTextFrame(--scrollCurPos, isFadeInRequired);
    else
        scrollCurPos = MATRIX_WIDTH;
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
        if (!scrollTextAktivLoop)
            RedrawAnimatedText(frameMillis);
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

    bool showDate = false;
    bool switchPage = false;
    if (clockSwitchAktiv && fromJSON)
    {
        clockCounterClock = clockSwitchSec > 0 ? 1 : 0;
        clockCounterDate = 0;
    }
    else if (clockSwitchAktiv)
    {
        if (clockCounterClock <= clockSwitchSec)
        {
            showDate = ++clockCounterClock > clockSwitchSec;
            switchPage = showDate;
            if (switchPage)
                clockCounterDate = 0;
        }
        else
        {
            switchPage = ++clockCounterDate >= clockSwitchSec;
            showDate = !switchPage;
            if (switchPage)
                clockCounterClock = 0;
        }
    }

    const bool transitionPrepared = switchPage && !fromJSON &&
        PrepareClockTransition(clockDateTransition, clockFontIsLarge, clockDrawWeekDays && !clockFontIsLarge);
    matrix->clear();
    const String text(showDate ? date : time);
    if (clockFontIsLarge)
        DrawTextHelper(text, clockFontChoice, true, false, false, false, clockColorR, clockColorG, clockColorB, 0, 1);
    else
        DrawText(text, false, clockColorR, clockColorG, clockColorB, showDate ? 7 : xPosTime, 1);

    if (!clockFontIsLarge && clockDrawWeekDays)
    {
        DrawWeekDay();
    }

    if (transitionPrepared)
        StartScreenTransition();
    else if (!fromJSON)
        ShowScreenContentFrame();
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


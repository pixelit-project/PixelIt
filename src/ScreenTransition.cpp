#include "FirmwareState.h"

namespace
{
enum class Effect : uint8_t
{
    None, Fade, ColoredBarWipe, ZigZagWipe, BitmapWipe, Curtain, PixelDissolve, Slide, Blinds, Crt, Glitch, VerticalSlide
};

constexpr uint32_t StepMillis = 24;
constexpr int LastStep = 16;
// Before rendering this holds the old screen. During playback it holds the
// target, while the regular LED buffer holds the latest composite frame.
CRGB transitionPixels[MATRIX_WIDTH * MATRIX_HEIGHT];
Effect effect = Effect::None;
bool preparing = false;
uint32_t startedAt;
int previousStep;
int verticalSlideRows = MATRIX_HEIGHT;
int fadeStepMillis = 10;
CRGB wipeColor;
int bitmapWidth;
uint16_t wipeBitmap[MATRIX_WIDTH * MATRIX_HEIGHT];

constexpr int FadeSteps = 26;

int WipeSteps()
{
    switch (effect)
    {
    case Effect::Fade: return FadeSteps;
    case Effect::ColoredBarWipe: return MATRIX_WIDTH + 1;
    case Effect::ZigZagWipe: return (MATRIX_WIDTH + 1) * (MATRIX_HEIGHT / 2);
    case Effect::BitmapWipe: return MATRIX_WIDTH + bitmapWidth - 1;
    default: return 0;
    }
}

int WipeStepMillis()
{
    switch (effect)
    {
    case Effect::Fade: return fadeStepMillis;
    case Effect::ColoredBarWipe: return 15;
    // A full LED transfer already takes roughly 8 ms on ESP8266.
    case Effect::ZigZagWipe: return 13;
    case Effect::BitmapWipe: return 18;
    default: return 0;
    }
}

void DrawWipeStep(int step)
{
    switch (effect)
    {
    case Effect::Fade:
    {
        const int remaining = FadeSteps - step;
        const int previousRemaining = FadeSteps - previousStep;
        for (auto &pixel : leds)
        {
            pixel.r = (pixel.r * remaining + previousRemaining / 2) / previousRemaining;
            pixel.g = (pixel.g * remaining + previousRemaining / 2) / previousRemaining;
            pixel.b = (pixel.b * remaining + previousRemaining / 2) / previousRemaining;
        }
        break;
    }
    case Effect::ColoredBarWipe:
    {
        const int x = step - 1;
        matrix->fillRect(0, 0, x, MATRIX_HEIGHT, 0);
        matrix->drawFastVLine(x, 0, MATRIX_HEIGHT, ColorWheel((x * 8) & 255, 0));
        matrix->drawFastVLine(x + 1, 0, MATRIX_HEIGHT, ColorWheel((x * 9) & 255, 0));
        break;
    }
    case Effect::ZigZagWipe:
    {
        const int row = (step - 1) / (MATRIX_WIDTH + 1) * 2;
        const int column = (step - 1) % (MATRIX_WIDTH + 1);
        const bool reverse = row % 4 != 0;
        const int x = reverse ? MATRIX_WIDTH - 1 - column : column;
        const int cleared = max(0, column - 1);
        matrix->fillRect(0, 0, MATRIX_WIDTH, row, 0);
        matrix->fillRect(reverse ? MATRIX_WIDTH - cleared : 0, row, cleared, 2, 0);
        const auto color = matrix->Color(wipeColor.r, wipeColor.g, wipeColor.b);
        matrix->drawFastVLine(x, row, 2, color);
        matrix->drawFastVLine(x + (reverse ? 1 : -1), row, 2, color);
        break;
    }
    case Effect::BitmapWipe:
    {
        const int x = step - bitmapWidth;
        matrix->fillRect(0, 0, max(0, x), MATRIX_HEIGHT, 0);
        for (int y = 0; y < MATRIX_HEIGHT; ++y)
            for (int column = max(0, x); column < min(MATRIX_WIDTH, x + bitmapWidth); ++column)
                matrix->drawPixel(column, y, wipeBitmap[y * bitmapWidth + column - x]);
        break;
    }
    default:
        break;
    }
}

Effect FindEffect(const char *name)
{
    if (!name) return Effect::None;
    if (strcmp(name, "fade") == 0) return Effect::Fade;
    if (strcmp(name, "coloredBarWipe") == 0) return Effect::ColoredBarWipe;
    if (strcmp(name, "zigzagWipe") == 0) return Effect::ZigZagWipe;
    if (strcmp(name, "curtain") == 0) return Effect::Curtain;
    if (strcmp(name, "pixelDissolve") == 0) return Effect::PixelDissolve;
    if (strcmp(name, "slide") == 0) return Effect::Slide;
    if (strcmp(name, "blinds") == 0) return Effect::Blinds;
    if (strcmp(name, "crt") == 0) return Effect::Crt;
    if (strcmp(name, "glitch") == 0) return Effect::Glitch;
    if (strcmp(name, "verticalSlide") == 0) return Effect::VerticalSlide;
    return Effect::None;
}

void DrawTransitionStep(int step)
{
    if (effect == Effect::VerticalSlide)
    {
        const int delta = step - previousStep;
        // Move down, reading each old row before it can be overwritten.
        for (int y = verticalSlideRows - 1; y >= 0; --y)
            for (int x = 0; x < MATRIX_WIDTH; ++x)
                leds[matrix->XY(x, y)] = y >= step
                    ? leds[matrix->XY(x, y - delta)]
                    : transitionPixels[matrix->XY(x, y + verticalSlideRows - step)];
        return;
    }

    const int slideOffset = step * MATRIX_WIDTH / LastStep;
    const int slideDelta = slideOffset - previousStep * MATRIX_WIDTH / LastStep;
    for (int y = 0; y < MATRIX_HEIGHT; ++y)
    {
        for (int x = 0; x < MATRIX_WIDTH; ++x)
        {
            const int index = matrix->XY(x, y);
            switch (effect)
            {
            case Effect::Curtain:
            {
                const int distance = min(x, MATRIX_WIDTH - 1 - x);
                if (step <= 8)
                {
                    if (distance < step * MATRIX_WIDTH / 16)
                        leds[index] = CRGB::Black;
                }
                else
                {
                    leds[index] = distance >= (LastStep - step) * MATRIX_WIDTH / 16
                        ? transitionPixels[index] : CRGB::Black;
                }
                break;
            }
            case Effect::PixelDissolve:
            {
                // An odd multiplier permutes all 256 positions on the matrix.
                const int rank = ((y * MATRIX_WIDTH + x) * 73) % (MATRIX_WIDTH * MATRIX_HEIGHT);
                if (rank < step * MATRIX_WIDTH * MATRIX_HEIGHT / LastStep)
                    leds[index] = transitionPixels[index];
                break;
            }
            case Effect::Slide:
                // Read from the right, which has yet to move in this pass.
                if (x < MATRIX_WIDTH - slideOffset)
                    leds[index] = leds[matrix->XY(x + slideDelta, y)];
                else
                    leds[index] = transitionPixels[matrix->XY(x + slideOffset - MATRIX_WIDTH, y)];
                break;
            case Effect::Blinds:
            {
                const int rank = (x % 4) * 2 + x / 4;
                if (rank < step)
                    leds[index] = transitionPixels[index];
                break;
            }
            case Effect::Crt:
            {
                const int distance = abs(y - MATRIX_HEIGHT / 2);
                if (step <= 8)
                {
                    if (distance > (8 - step) * MATRIX_HEIGHT / 16)
                        leds[index] = CRGB::Black;
                }
                else
                {
                    leds[index] = distance <= (step - 9) * MATRIX_HEIGHT / 14
                        ? transitionPixels[index] : CRGB::Black;
                }
                break;
            }
            case Effect::Glitch:
            {
                // Skew each old row once, then settle the new rows at
                // staggered times. Read old rows rightward before overwriting.
                const int tear = 1 + (y * 5) % 7;
                if (step < 8)
                {
                    if (previousStep < tear && step >= tear)
                    {
                        const int sourceX = x + 2 + y % 4;
                        leds[index] = sourceX < MATRIX_WIDTH ? leds[matrix->XY(sourceX, y)] : CRGB::Black;
                    }
                }
                else
                {
                    const int offset = step < 8 + tear ? ((step + y) % 2 ? 3 : -4) : 0;
                    const int sourceX = x + offset;
                    leds[index] = sourceX >= 0 && sourceX < MATRIX_WIDTH
                        ? transitionPixels[matrix->XY(sourceX, y)] : CRGB::Black;
                }
                break;
            }
            default:
                break;
            }
        }
    }
}

void PrepareTransition(Effect selected)
{
    memcpy(transitionPixels, leds, sizeof(transitionPixels));
    effect = selected;
    preparing = true;
}
}

bool PrepareScreenTransition(const char *name, CRGB color, bool hasTarget)
{
    const Effect selected = FindEffect(name);
    if (selected == Effect::None || firmwareUpdateDisplayActive ||
        (!hasTarget && selected != Effect::Fade && selected != Effect::ColoredBarWipe && selected != Effect::ZigZagWipe))
        return false;

    PrepareTransition(selected);
    wipeColor = color;
    fadeStepMillis = 10;
    verticalSlideRows = MATRIX_HEIGHT;
    if (!hasTarget && selected != Effect::Fade)
        matrix->clear();
    return true;
}

bool PrepareClockTransition(unsigned selection, bool largeFont, bool keepWeekday)
{
    static const char *const transitions[] = {
        nullptr, "fade", "coloredBarWipe", "zigzagWipe", nullptr,
        "curtain", "pixelDissolve", "slide", "blinds", "crt", "glitch", "verticalSlide"
    };
    static_assert(ClockFallbackLastEffect + 1 == sizeof(transitions) / sizeof(transitions[0]),
                  "Clock transition settings must match the effect list");
    const bool automatic = selection == ClockDateTransitionAuto;
    if (automatic)
        selection = largeFont ? 1 : 11;
    else if (selection == 4)
    {
        selection = random(1, ClockFallbackLastEffect);
        if (selection >= 4)
            ++selection;
    }
    if (selection > ClockFallbackLastEffect ||
        !PrepareScreenTransition(transitions[selection], CRGB(clockColorR, clockColorG, clockColorB)))
        return false;
    if (automatic && largeFont)
        fadeStepMillis = 30;
    if (keepWeekday && effect == Effect::VerticalSlide)
        verticalSlideRows = MATRIX_HEIGHT - 1;
    return true;
}

bool PrepareBitmapWipe(const JsonArray &data, int width, bool hasTarget)
{
    if (firmwareUpdateDisplayActive || width < 1 || width > MATRIX_WIDTH ||
        data.size() != static_cast<size_t>(width * MATRIX_HEIGHT))
        return false;

    size_t index = 0;
    for (JsonVariant value : data)
        wipeBitmap[index++] = value.as<uint16_t>();
    bitmapWidth = width;
    fadeStepMillis = 10;
    PrepareTransition(Effect::BitmapWipe);
    matrix->clear();
    // The legacy effect-only wipe leaves the last visible sprite column.
    if (!hasTarget)
        for (int y = 0; y < MATRIX_HEIGHT; ++y)
            matrix->drawPixel(MATRIX_WIDTH - 1, y, wipeBitmap[y * width]);
    return true;
}

void StartScreenTransition()
{
    for (size_t i = 0; i < MATRIX_WIDTH * MATRIX_HEIGHT; ++i)
    {
        const CRGB target = leds[i];
        leds[i] = transitionPixels[i];
        transitionPixels[i] = target;
    }
    previousStep = 0;
    startedAt = millis();
    preparing = false;
}

void CancelScreenTransition()
{
    effect = Effect::None;
    preparing = false;
}

bool ScreenTransitionActive()
{
    return effect != Effect::None;
}

bool ServiceScreenTransition()
{
    if (!ScreenTransitionActive())
        return false;
    if (preparing)
        return true;

    const uint32_t nowMillis = millis();
    const uint32_t elapsed = nowMillis - startedAt;
    const int wipeSteps = WipeSteps();
    const int wipeDuration = wipeSteps * WipeStepMillis();
    const bool verticalSlide = effect == Effect::VerticalSlide;
    const int lastStep = verticalSlide ? verticalSlideRows : LastStep;
    const uint32_t step = !wipeSteps ? elapsed / (verticalSlide ? 35 : StepMillis)
        : elapsed < static_cast<uint32_t>(wipeDuration) ? elapsed / WipeStepMillis()
        : wipeSteps + (elapsed - wipeDuration) / fadeStepMillis;
    if (step <= static_cast<uint32_t>(previousStep))
        return true;

    if (step >= static_cast<uint32_t>(wipeSteps ? wipeSteps + FadeSteps : lastStep))
    {
        memcpy(leds, transitionPixels, sizeof(transitionPixels));
        matrix->show();
        CancelScreenTransition();
        // AnimateBMP(false) prepared the first frame before the transition.
        // Its playback interval starts when that frame reaches the LEDs.
        animationLastRenderedMillis = nowMillis;
        animateBMPPrevMillis = scrollTextPrevMillis = nowMillis;
    }
    else
    {
        if (!wipeSteps)
            DrawTransitionStep(step);
        else if (step < static_cast<uint32_t>(wipeSteps))
            DrawWipeStep(step);
        else
        {
            const int brightness = step - wipeSteps;
            for (size_t i = 0; i < MATRIX_WIDTH * MATRIX_HEIGHT; ++i)
            {
                const auto &target = transitionPixels[i];
                leds[i] = CRGB(target.r * brightness / FadeSteps,
                               target.g * brightness / FadeSteps,
                               target.b * brightness / FadeSteps);
            }
        }
        matrix->show();
        previousStep = step;
    }
    return true;
}

void ShowScreenContentFrame()
{
    if (!preparing)
        matrix->show();
}

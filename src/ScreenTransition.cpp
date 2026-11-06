#include "FirmwareState.h"

namespace
{
enum class Effect : uint8_t
{
    None, Curtain, PixelDissolve, Slide, Blinds, Crt, Glitch
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

Effect FindEffect(const char *name)
{
    if (!name) return Effect::None;
    if (strcmp(name, "curtain") == 0) return Effect::Curtain;
    if (strcmp(name, "pixelDissolve") == 0) return Effect::PixelDissolve;
    if (strcmp(name, "slide") == 0) return Effect::Slide;
    if (strcmp(name, "blinds") == 0) return Effect::Blinds;
    if (strcmp(name, "crt") == 0) return Effect::Crt;
    if (strcmp(name, "glitch") == 0) return Effect::Glitch;
    return Effect::None;
}

void DrawTransitionStep(int step)
{
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
            case Effect::None:
                break;
            }
        }
    }
}
}

bool PrepareScreenTransition(const char *name)
{
    const Effect selected = FindEffect(name);
    if (selected == Effect::None || firmwareUpdateDisplayActive)
        return false;

    memcpy(transitionPixels, leds, sizeof(transitionPixels));
    effect = selected;
    preparing = true;
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
    const uint32_t elapsedSteps = (nowMillis - startedAt) / StepMillis;
    if (elapsedSteps <= static_cast<uint32_t>(previousStep))
        return true;

    if (elapsedSteps >= LastStep)
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
        const int step = elapsedSteps;
        DrawTransitionStep(step);
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

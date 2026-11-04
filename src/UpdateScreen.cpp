#include "FirmwareState.h"
#include "UpdateScreen.h"
#include "ScreenJsonCompactor.h"

bool BuildUpdateScreenJSON(JsonObject &root)
{
    JsonObject &text = root.createNestedObject("text");
    JsonObject &position = text.createNestedObject("position");
    JsonObject &animation = root.createNestedObject("bitmapAnimation");
    JsonObject &size = animation.createNestedObject("size");
    JsonArray &frames = animation.createNestedArray("data");
    if (!text.success() || !position.success() || !animation.success() ||
        !size.success() || !frames.success() ||
        !text.set("textString", "New FW available") ||
        !text.set("hexColor", "#FFFFFF") || !text.set("scrollText", true) ||
        !position.set("x", 8) || !position.set("y", 1) ||
        !animation.set("animationDelay", 200) || !animation.set("limitLoops", 0) ||
        !size.set("width", 8) || !size.set("height", 8))
        return false;

    // Reuse the existing internal pixel encoding instead of allocating a JSON
    // node per pixel. Mutable strings are copied into the JSON buffer by v5.
    for (uint8_t phase = 0; phase < 8; ++phase)
    {
        char encoded[1 + 64 * 3 + 1];
        size_t length = 0;
        encoded[length++] = '$';
        for (uint8_t y = 0; y < 8; ++y)
            for (uint8_t x = 0; x < 8; ++x)
            {
                const uint16_t pixel = UpdateArrowPixel(phase, x, y);
                if (pixel <= 47)
                    encoded[length++] = ScreenPixelDigit(16 + pixel);
                else
                {
                    encoded[length++] = ScreenPixelDigit(pixel >> 12);
                    encoded[length++] = ScreenPixelDigit((pixel >> 6) & 63);
                    encoded[length++] = ScreenPixelDigit(pixel & 63);
                }
            }
        encoded[length] = '\0';
        if (!frames.add(encoded)) return false;
    }
    return true;
}

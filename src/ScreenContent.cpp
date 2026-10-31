#include "FirmwareState.h"
#include "Tools.h"

void CreateFrames(JsonObject &json)
{
    CreateFrames(json, 0);
}

void CreateFrames(JsonObject &json, int forceDuration)
{
    if (!PrepareBitmapAnimation(json))
    {
        Log(F("BitmapAnimation"), F("Rejected: invalid dimensions, pixels or frame/pixel budget (28 frames, 1792 pixels total)"));
        return;
    }

    bool sendMatrixInfo = false;

    String logMessage = F("JSON contains ");

    if (json.containsKey("sleepMode"))
    {

        logMessage += F("Sleep Control, ");
        Serial.printf("SleepMode: %s\n", json["sleepMode"].as<bool>() ? "true" : "false");

        // Update internal sleep state
        bool newSleepMode = json["sleepMode"].as<bool>();

        if (newSleepMode != sleepMode)
        {
            sendMatrixInfo = true;
            // Update sleep mode
            sleepMode = newSleepMode;

            // Display sleepscreen
            SleepScreen(sleepMode, false);

            // Restore last frame if sleep mode is disabled
            if (sleepMode == false && currentScreenJsonBuffer.length() > 0)
            {
                DynamicJsonBuffer jsonBuffer;
                JsonObject &tmpJson = jsonBuffer.parseObject(currentScreenJsonBuffer);
                CreateFrames(tmpJson);
                return;
            }
        }
    }

    // Ist eine Display Helligkeit übergeben worden?
    if (json.containsKey("brightness") && !sleepMode)
    {
        logMessage += F("Brightness Control, ");

        if (json["brightness"].as<int>() != currentMatrixBrightness)
        {
            sendMatrixInfo = true;
            currentMatrixBrightness = json["brightness"].as<int>();
            matrix->setBrightness(currentMatrixBrightness);
            matrix->show();
        }
    }

    // Enable/Disable automatic brightness control
    if (json.containsKey("autobrightness"))
    {
        logMessage += F("Brightness Automatic Control, ");
        if (json["autobrightness"].as<bool>() != matrixBrightnessAutomatic)
        {
            sendMatrixInfo = true;
            matrixBrightnessAutomatic = json["autobrightness"].as<bool>();
        }
    }

    // Set GPIO
    if (json.containsKey("setGpio"))
    {
        logMessage += F("Set Gpio, ");
        if (json["setGpio"]["set"].is<bool>() && json["setGpio"]["gpio"].is<uint8_t>())
        {
            uint8_t gpio = json["setGpio"]["gpio"].as<uint8_t>();

            // If the GPIO is already present in the array?
            // has been found, this is to be replaced.
            if (json["setGpio"]["duration"].is<ulong>())
            {
                int arrayIndex = -1;
                for (int i = 0; i < SET_GPIO_SIZE; i++)
                {
                    if (setGPIOReset[i].gpio == gpio)
                    {
                        arrayIndex = i;
                        break;
                    }
                }
                // Search free place in array.
                if (arrayIndex == -1)
                {
                    for (int i = 0; i < SET_GPIO_SIZE; i++)
                    {
                        if (setGPIOReset[i].gpio == -1)
                        {
                            arrayIndex = i;
                            break;
                        }
                    }
                }

                if (arrayIndex == -1)
                {
                    Log(F("SetGPIO"), F("Error: no free place in array found!"));
                }
                else
                {
                    // Save data in array for the reset.
                    setGPIOReset[arrayIndex].gpio = gpio;
                    setGPIOReset[arrayIndex].resetMillis = (millis() + json["setGpio"]["duration"].as<ulong>());
                    Log(F("SetGPIO"), "Pos: " + String(arrayIndex) + ", GPIO: " + String(gpio) + ", Duration: " + String(json["setGpio"]["duration"].as<char *>()) + ", Value: " + json["setGpio"]["set"].as<char *>());
                }
            }
            else
            {
                Log(F("SetGPIO"), "GPIO: " + String(gpio) + ", Value: " + json["setGpio"]["set"].as<char *>());
            }
            // Set GPIO
            pinMode(gpio, OUTPUT);
            digitalWrite(gpio, json["setGpio"]["set"].as<bool>());
        }
    }

    // Sound
    if (json.containsKey("sound"))
    {
        logMessage += F("Sound, ");
        // Volume
        if (json["sound"]["volume"] != NULL && json["sound"]["volume"].is<int>())
        {
            mp3Player.volume(json["sound"]["volume"].as<int>());

            // Sometimes, mp3Player gets hickups. A brief delay might help - but also might hinder scrolling.
            // So, do it only if there are more commands to come.
            if (json["sound"]["control"].as<String>() == "")
            {
                Log(F("Sound"), F("Changing volume can prevent DFPlayer from executing a control command at the same time. Better make two separate API calls."));
                delay(200);
            }
        }
        // Play
        if (json["sound"]["control"] == "play")
        {
            if (json["sound"]["folder"])
            {
                mp3Player.playFolder(json["sound"]["folder"].as<int>(), json["sound"]["file"].as<int>());
            }
            else
            {
                mp3Player.play(json["sound"]["file"].as<int>());
            }
        }
        // Stop
        else if (json["sound"]["control"] == "pause")
        {
            mp3Player.pause();
        }
        // Play Next
        else if (json["sound"]["control"] == "next")
        {
            mp3Player.playNext();
        }
        // Play Previous
        else if (json["sound"]["control"] == "previous")
        {
            mp3Player.playPrevious();
        }
    }

    // Process screen content if
    // - no sleepmode control is in message
    // - not sleeping
    // - no forced screen is active OR forceDuration is set
    if (!json.containsKey("sleepMode") && !sleepMode && (millis() >= forcedScreenIsActiveUntil || forceDuration > 0))
    {
        matrix->setBrightness(currentMatrixBrightness);

        // Prüfung für die Unterbrechnung der lokalen Schleifen
        if (json.containsKey("bitmap") || json.containsKey("bitmaps") || json.containsKey("text") || json.containsKey("bar") || json.containsKey("bars") || json.containsKey("bitmapAnimation"))
        {
            lastScreenMessageMillis = millis();
            clockAktiv = false;
            scrollTextAktivLoop = false;
            animateBMPAktivLoop = false;
        }

        // Ist eine Switch Animation übergeben worden?
        bool fadeAnimationAktiv = false;
        bool coloredBarWipeAnimationAktiv = false;
        bool zigzagWipeAnimationAktiv = false;
        bool bitmapWipeAnimationAktiv = false;
        if (json.containsKey("switchAnimation"))
        {
            logMessage += F("SwitchAnimation, ");
            // Switch Animation aktiv?
            if (json["switchAnimation"]["aktiv"])
            {
                // Fade Animation aktiv?
                if (json["switchAnimation"]["animation"] == "fade")
                {
                    fadeAnimationAktiv = true;
                }
                else if (json["switchAnimation"]["animation"] == "coloredBarWipe")
                {
                    coloredBarWipeAnimationAktiv = true;
                }
                else if (json["switchAnimation"]["animation"] == "zigzagWipe")
                {
                    zigzagWipeAnimationAktiv = true;
                }
                else if (json["switchAnimation"]["animation"] == "bitmapWipe")
                {
                    bitmapWipeAnimationAktiv = true;
                }
                else if (json["switchAnimation"]["animation"] == "random")
                {
                    switch (millis() % 3)
                    {
                    case 0:
                        fadeAnimationAktiv = true;
                        break;
                    case 1:
                        coloredBarWipeAnimationAktiv = true;
                        break;
                    case 2:
                        zigzagWipeAnimationAktiv = true;
                        break;
                    }
                }
            }
        }

        // Fade aktiv?
        if (fadeAnimationAktiv)
        {
            FadeOut();
        }
        else if (coloredBarWipeAnimationAktiv)
        {
            ColoredBarWipe();
        }
        else if (zigzagWipeAnimationAktiv)
        {
            uint8_t r = 255;
            uint8_t g = 255;
            uint8_t b = 255;
            if (json["switchAnimation"]["hexColor"].as<char *>() != NULL)
            {
                HEXtoRGB(json["switchAnimation"]["hexColor"].as<char *>(), r, g, b);
            }
            else if (json["switchAnimation"]["color"]["r"].as<char *>() != NULL)
            {
                r = json["switchAnimation"]["color"]["r"].as<uint8_t>();
                g = json["switchAnimation"]["color"]["g"].as<uint8_t>();
                b = json["switchAnimation"]["color"]["b"].as<uint8_t>();
            }
            ZigZagWipe(r, g, b);
        }
        else if (bitmapWipeAnimationAktiv)
        {
            BitmapWipe(json["switchAnimation"]["data"].as<JsonArray>(), json["switchAnimation"]["width"].as<uint8_t>());
        }

        // Clock
        if (json.containsKey("clock"))
        {
            logMessage += F("InternalClock Control, Params: (");
            scrollTextAktivLoop = false;
            animateBMPAktivLoop = false;
            clockAktiv = true;

            clockCounterClock = 0;
            clockCounterDate = 0;

            bool isSwitchAktivSet = json["clock"]["switchAktiv"].is<bool>();
            if (isSwitchAktivSet)
            {
                logMessage += F("clockSwitchAktiv, ");
                clockSwitchAktiv = json["clock"]["switchAktiv"];
            }

            bool isClockSwitchSecSet = json["clock"]["switchSec"] != NULL;
            if (clockSwitchAktiv && isClockSwitchSecSet)
            {
                logMessage += F("clockSwitchSec, ");
                clockSwitchSec = json["clock"]["switchSec"];
            }

            bool isClockWithSecondsSet = json["clock"]["withSeconds"].is<bool>();
            if (isClockWithSecondsSet)
            {
                logMessage += F("withSeconds, ");
                clockWithSeconds = json["clock"]["withSeconds"];
            }

            bool isClockBlinkAnimatedSet = json["clock"]["blinkAnimated"].is<bool>();
            if (isClockBlinkAnimatedSet)
            {
                logMessage += F("blinkAnimated, ");
                clockBlinkAnimated = json["clock"]["blinkAnimated"];
            }

            bool isFatFontSet = json["clock"]["fatFont"].is<bool>();
            if (isFatFontSet)
            {
                logMessage += F("fatFont, ");
                clockFatFont = json["clock"]["fatFont"];
            }
            bool isLargeFontSet = json["clock"]["largeFont"].is<bool>();
            if (isLargeFontSet)
            {
                logMessage += F("largeFont, ");
                clockLargeFont = json["clock"]["largeFont"];
            }
            bool isDrawWeekDaysSet = json["clock"]["drawWeekDays"].is<bool>();
            if (isDrawWeekDaysSet)
            {
                logMessage += F("drawWeekDays, ");
                clockDrawWeekDays = json["clock"]["drawWeekDays"];
            }

            if (json["clock"]["color"]["r"].as<char *>() != NULL)
            {
                logMessage += F("color, ");
                clockColorR = json["clock"]["color"]["r"].as<uint8_t>();
                clockColorG = json["clock"]["color"]["g"].as<uint8_t>();
                clockColorB = json["clock"]["color"]["b"].as<uint8_t>();
            }
            else if (json["clock"]["hexColor"].as<char *>() != NULL)
            {
                logMessage += F("hexColor, ");
                HEXtoRGB(json["clock"]["hexColor"].as<char *>(), clockColorR, clockColorG, clockColorB);
            }
            if (logMessage.endsWith(", "))
            {
                logMessage.remove(logMessage.length() - 2);
            }
            logMessage += F("), ");
            DrawClock(true);
        }

        if (json.containsKey("bitmap") || json.containsKey("bitmaps") || json.containsKey("bitmapAnimation") || json.containsKey("text") || json.containsKey("bar") || json.containsKey("bars"))
        {
            // Alle Pixel löschen
            matrix->clear();
        }

        // Bar
        if (json.containsKey("bar"))
        {
            logMessage += F("Bar, ");
            uint8_t r, g, b;
            if (json["bar"]["hexColor"].as<char *>() != NULL)
            {
                HEXtoRGB(json["bar"]["hexColor"].as<char *>(), r, g, b);
            }
            else
            {
                r = json["bar"]["color"]["r"].as<uint8_t>();
                g = json["bar"]["color"]["g"].as<uint8_t>();
                b = json["bar"]["color"]["b"].as<uint8_t>();
            }
            matrix->drawLine(json["bar"]["position"]["x"], json["bar"]["position"]["y"], json["bar"]["position"]["x2"], json["bar"]["position"]["y2"], matrix->Color(r, g, b));
        }

        // Bars
        if (json.containsKey("bars"))
        {
            logMessage += F("Bars, ");
            for (JsonVariant x : json["bars"].as<JsonArray>())
            {
                uint8_t r, g, b;
                if (x["hexColor"].as<char *>() != NULL)
                {
                    HEXtoRGB(x["hexColor"].as<char *>(), r, g, b);
                }
                else
                {
                    r = x["color"]["r"].as<uint8_t>();
                    g = x["color"]["g"].as<uint8_t>();
                    b = x["color"]["b"].as<uint8_t>();
                }
                matrix->drawLine(x["position"]["x"], x["position"]["y"], x["position"]["x2"], x["position"]["y2"], matrix->Color(r, g, b));
            }
        }

        // clear withBMP only if no sleepMode is in the same message
        if (!json.containsKey("sleepMode"))
        {
            withBMP = false;
        }

        // Restore withBMP from (stored) message
        if (json.containsKey("withBMPRestore"))
        {
            withBMP = json["withBMPRestore"];
            if (withBMP && bmpArray)
            {
                matrix->drawRGBBitmap(bmpPosX, bmpPosY, bmpArray.get(), bmpWidth, bmpHeight);
            }
        }

        // Restore a animateBMPAktivLoop from (stored) message
        if (json.containsKey("animateBMPAktivLoopRestore"))
        {
            animateBMPAktivLoop = json["animateBMPAktivLoopRestore"];
        }

        // Ist ein Bitmap übergeben worden?
        if (json.containsKey("bitmap"))
        {
            logMessage += F("Bitmap, ");
            DrawSingleBitmap(json["bitmap"]);
        }

        // Sind mehrere Bitmaps übergeben worden?
        if (json.containsKey("bitmaps"))
        {
            logMessage += F("Bitmaps (");
            for (JsonVariant singleBitmap : json["bitmaps"].as<JsonArray>())
            {
                DrawSingleBitmap(singleBitmap);
            }
            logMessage += json["bitmaps"].as<JsonArray>().size();
            logMessage += F("), ");
        }

        // Ist eine BitmapAnimation übergeben worden?
        if (json.containsKey("bitmapAnimation"))
        {
            bmpPosX = 0;
            bmpPosY = 0;
            bmpWidth = 8;
            bmpHeight = 8;
            if (json["bitmapAnimation"]["position"]["x"].is<int16_t>() && json["bitmapAnimation"]["position"]["y"].is<int16_t>())
            {
                bmpPosX = json["bitmapAnimation"]["position"]["x"].as<int16_t>();
                bmpPosY = json["bitmapAnimation"]["position"]["y"].as<int16_t>();
            }
            if (json["bitmapAnimation"]["size"]["width"].is<int16_t>() && json["bitmapAnimation"]["size"]["height"].is<int16_t>())
            {
                bmpWidth = json["bitmapAnimation"]["size"]["width"].as<int16_t>();
                bmpHeight = json["bitmapAnimation"]["size"]["height"].as<int16_t>();
            }
            withBMP = true;

            logMessage += F("BitmapAnimation, ");
            JsonArray &frames = json["bitmapAnimation"]["data"].as<JsonArray>();
            animationBmpCount = frames.size();
            animationBmpPixels = static_cast<size_t>(bmpWidth) * bmpHeight;
            size_t counter = 0;
            for (JsonVariant frame : frames)
            {
                uint16_t *storedFrame = animationBmpList.get() + counter * animationBmpPixels;
                if (frame.is<const char *>())
                {
                    const char *pixels = frame.as<const char *>();
                    DecodeAnimationPixels(pixels, strlen(pixels), storedFrame);
                }
                else
                {
                    frame.as<JsonArray>().copyTo(storedFrame, animationBmpPixels);
                }
                counter++;
            }

            animateBMPDelay = json["bitmapAnimation"]["animationDelay"];
            animateBMPRubberbandingAktiv = json["bitmapAnimation"]["rubberbanding"];

            animateBMPLimitLoops = 0;
            if (json["bitmapAnimation"]["limitLoops"])
            {
                animateBMPLimitLoops = json["bitmapAnimation"]["limitLoops"].as<int>();
            }

            // Hier einmal den Counter zurücksetzten
            animateBMPCounter = 0;
            animateBMPLoopCount = 0;
            animateBMPAktivLoop = true;
            animateBMPReverse = false;
            animationRenderedFrames = 0;
            animationFailedLedFrames = 0;
            animationSlowestTaskMillis = 0;
            animationSlowestTask = "none";
            animationMaxFrameGap = 0;
            animateBMPPrevMillis = millis();
            AnimateBMP(false);
        }

        // Ist ein Text übergeben worden?
        bool scrollTextAktiv = false;
        if (json.containsKey("text"))
        {
            haDisplayText = json["text"]["textString"].as<String>();
            logMessage += F("Text, ");
            // Always assume the default delay first.
            scrollTextDelay = scrollTextDefaultDelay;

            // Is ScrollText auto or true selected?
            scrollTextAktiv = ((json["text"]["scrollText"] == "auto" || ((json["text"]["scrollText"]).is<bool>() && json["text"]["scrollText"])));

            uint8_t r, g, b;
            if (json["text"]["hexColor"].as<char *>() != NULL)
            {
                HEXtoRGB(json["text"]["hexColor"].as<char *>(), r, g, b);
            }
            else
            {
                r = json["text"]["color"]["r"].as<uint8_t>();
                g = json["text"]["color"]["g"].as<uint8_t>();
                b = json["text"]["color"]["b"].as<uint8_t>();
            }

            // Is ScrollText auto or true selected?
            if (scrollTextAktiv)
            {

                bool centerText = json["text"]["centerText"];

                bool fadeInRequired = ((json.containsKey("bars") || json.containsKey("bar") || json.containsKey("bitmap") || json.containsKey("bitmapAnimation")) && fadeAnimationAktiv);

                // Wurde ein Benutzerdefeniertes Delay übergeben?
                if (json["text"]["scrollTextDelay"])
                {
                    scrollTextDelay = json["text"]["scrollTextDelay"];
                }

                if (!(json["text"]["scrollText"]).is<bool>() && json["text"]["scrollText"] == "auto")
                {
                    DrawAutoTextScrolled(json["text"]["textString"], json["text"]["bigFont"], centerText, fadeInRequired, r, g, b, json["text"]["position"]["x"], json["text"]["position"]["y"]);
                }
                else
                {
                    DrawTextScrolled(json["text"]["textString"], json["text"]["bigFont"], centerText, fadeInRequired, r, g, b, json["text"]["position"]["x"], json["text"]["position"]["y"]);
                }
            }
            // is centerText selected?
            else if (json["text"]["centerText"])
            {
                DrawTextCenter(json["text"]["textString"], json["text"]["bigFont"], r, g, b, json["text"]["position"]["x"], json["text"]["position"]["y"]);
            }
            else
            {
                DrawText(json["text"]["textString"], json["text"]["bigFont"], r, g, b, json["text"]["position"]["x"], json["text"]["position"]["y"]);
            }
        }

        // Fade aktiv?
        if (!scrollTextAktiv && (fadeAnimationAktiv || coloredBarWipeAnimationAktiv || zigzagWipeAnimationAktiv || bitmapWipeAnimationAktiv))
        {
            FadeIn();
        }
        else
        {
            // Fade nicht aktiv!
            // Muss mich selbst um Show kümmern
            matrix->show();
        }
    }

    if (sleepMode && !json.containsKey("sleepMode"))
    {
        logMessage += F("[not all data processed because sleepMode is active]");
    }

    if (logMessage.endsWith(", "))
    {
        logMessage.remove(logMessage.length() - 2);
        logMessage += F(" ");
    }
    Log(F("CreateFrames"), logMessage + "(Length: " + json.measureLength() + ")");

    if (forceDuration > 0 && (json.containsKey("bitmap") || json.containsKey("bitmaps") || json.containsKey("text") || json.containsKey("bar") || json.containsKey("bars") || json.containsKey("bitmapAnimation")))
    {
        forcedScreenIsActiveUntil = millis() + forceDuration;
    }

    if (!json.containsKey("sleepMode") && !sleepMode)
    {
        // Store last frame

        json.remove("bitmap");
        json.remove("bitmaps");
        json.remove("bitmapAnimation");
        json["withBMPRestore"] = withBMP;
        json["animateBMPAktivLoopRestore"] = animateBMPAktivLoop;
        currentScreenJsonBuffer = "";
        json.printTo(currentScreenJsonBuffer);
    }

    if (sendMatrixInfo)
    {
        SendMatrixInfo();
    }
}


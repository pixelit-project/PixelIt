#include "FirmwareState.h"
#include "Tools.h"

void SaveConfig()
{
    // save the custom parameters to FS
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.createObject();

    json["version"] = VERSION;
    json["isESP8266"] = isESP8266;
    json["temperatureUnit"] = static_cast<int>(temperatureUnit);
    json["matrixBrightnessAutomatic"] = matrixBrightnessAutomatic;
    json["mbaDimMin"] = mbaDimMin;
    json["mbaDimMax"] = mbaDimMax;
    json["mbaLuxMin"] = mbaLuxMin;
    json["mbaLuxMax"] = mbaLuxMax;
    json["matrixBrightness"] = currentMatrixBrightness;
    json["matrixType"] = matrixType;
    json["note"] = note;
    json["hostname"] = hostname;
    json["matrixTempCorrection"] = matrixTempCorrection;
    json["ntpServer"] = ntpServer;
    json["clockTimeZone"] = clockTimeZone;
    json["clockColor"] = "#" + RGBtoHEX(clockColorR, clockColorG, clockColorB);
    json["clockSwitchAktiv"] = clockSwitchAktiv;
    json["clockSwitchSec"] = clockSwitchSec;
    json["clock24Hours"] = clock24Hours;
    json["clockDayLightSaving"] = clockDayLightSaving;
    json["clockWithSeconds"] = clockWithSeconds;
    json["clockAutoFallbackActive"] = clockAutoFallbackActive;
    json["clockAutoFallbackTime"] = clockAutoFallbackTime;
    json["clockAutoFallbackAnimation"] = clockAutoFallbackAnimation;
    json["clockDateDayMonth"] = clockDateDayMonth;
    json["clockDayOfWeekFirstMonday"] = clockDayOfWeekFirstMonday;
    json["clockBlinkAnimated"] = clockBlinkAnimated;
    json["clockLargeFont"] = clockLargeFont;
    json["clockFatFont"] = clockFatFont;
    json["clockDrawWeekDays"] = clockDrawWeekDays;
    json["scrollTextDefaultDelay"] = scrollTextDefaultDelay;
    json["bootScreenAktiv"] = bootScreenAktiv;
    json["bootBatteryScreen"] = bootBatteryScreen;
    json["bootSound"] = bootSound;
    json["mqttAktiv"] = mqttAktiv;
    json["mqttUser"] = mqttUser;
    json["mqttPassword"] = mqttPassword;
    json["mqttServer"] = mqttServer;
    json["mqttMasterTopic"] = mqttMasterTopic;
    json["mqttPort"] = mqttPort;
    json["mqttUseDeviceTopic"] = mqttUseDeviceTopic;
    json["mqttHAdiscoverable"] = mqttHAdiscoverable;
    json["luxOffset"] = luxOffset;
    json["temperatureOffset"] = temperatureOffset;
    json["humidityOffset"] = humidityOffset;
    json["pressureOffset"] = pressureOffset;
    json["gasOffset"] = gasOffset;

    json["dfpRXpin"] = dfpRXPin;
    json["dfpTXpin"] = dfpTXPin;
    json["onewirePin"] = onewirePin;
    json["SCLPin"] = SCLPin;
    json["SDAPin"] = SDAPin;
    for (uint b = 0; b < 3; b++)
    {
        json["btn" + String(b) + "Pin"] = btnPin[b];
        json["btn" + String(b) + "PressedLevel"] = btnPressedLevel[b];
        json["btn" + String(b) + "Enabled"] = btnEnabled[b];
        json["btn" + String(b) + "Action"] = static_cast<int>(btnAction[b]);
    }
    json["ldrDevice"] = ldrDevice;
    json["ldrPulldown"] = ldrPulldown;
    json["ldrSmoothing"] = ldrSmoothing;
    json["initialVolume"] = initialVolume;
    json["sendTelemetry"] = sendTelemetry;
    json["checkUpdateScreen"] = checkUpdateScreen;

#if defined(ESP8266)
    File configFile = LittleFS.open("/config.json", "w");
#elif defined(ESP32)
    File configFile = SPIFFS.open("/config.json", "w");
#endif
    json.printTo(configFile);
    configFile.close();
    Log("SaveConfig", "Saved");
    // end save
}

void LoadConfig()
{
    // file exists, reading and loading
#if defined(ESP8266)
    if (LittleFS.exists("/config.json"))
    {
        File configFile = LittleFS.open("/config.json", "r");
#elif defined(ESP32)
    if (SPIFFS.exists("/config.json"))
    {
        File configFile = SPIFFS.open("/config.json", "r");
#endif
        if (configFile)
        {
            // Serial.println("opened config file");

            DynamicJsonBuffer jsonBuffer;
            JsonObject &json = jsonBuffer.parseObject(configFile);

            if (json.success())
            {
                SetConfigVariables(json);
                Log("LoadConfig", "Loaded");
            }
        }
    }
    else
    {
        Log("LoadConfig", "No Configfile, init new file");
        SaveConfig();
    }
}

void SetConfig(JsonObject &json)
{
    SetConfigVariables(json);
    SaveConfig();
    RequestHADiscovery();
}

void SetConfigVariables(JsonObject &json)
{
    if (json.containsKey("version"))
    {
        optionsVersion = json["version"].as<String>();
    }

    if (json.containsKey("temperatureUnit"))
    {
        temperatureUnit = static_cast<TemperatureUnit>(json["temperatureUnit"].as<int>());
    }

    if (json.containsKey("matrixBrightnessAutomatic"))
    {
        matrixBrightnessAutomatic = json["matrixBrightnessAutomatic"].as<bool>();
    }

    if (json.containsKey("mbaDimMin"))
    {
        mbaDimMin = json["mbaDimMin"].as<int>();
    }

    if (json.containsKey("mbaDimMax"))
    {
        mbaDimMax = json["mbaDimMax"].as<int>();
    }

    if (json.containsKey("mbaLuxMin"))
    {
        mbaLuxMin = json["mbaLuxMin"].as<int>();
    }

    if (json.containsKey("mbaLuxMax"))
    {
        mbaLuxMax = json["mbaLuxMax"].as<int>();
    }

    if (json.containsKey("matrixBrightness"))
    {
        SetCurrentMatrixBrightness(json["matrixBrightness"].as<int>());
    }

    if (json.containsKey("matrixType"))
    {
        matrixType = json["matrixType"].as<int>();
    }

    if (json.containsKey("note"))
    {
        note = json["note"].as<char *>();
    }

    if (json.containsKey("hostname"))
    {
        String hostname_raw = json["hostname"].as<char *>();
        hostname = "";
        for (uint8_t n = 0; n < hostname_raw.length(); n++)
        {
            if ((hostname_raw.charAt(n) >= '0' && hostname_raw.charAt(n) <= '9') || (hostname_raw.charAt(n) >= 'A' && hostname_raw.charAt(n) <= 'Z') || (hostname_raw.charAt(n) >= 'a' && hostname_raw.charAt(n) <= 'z') || (hostname_raw.charAt(n) == '_') || (hostname_raw.charAt(n) == '-'))
                hostname += hostname_raw.charAt(n);
        }
    }

    if (json.containsKey("matrixTempCorrection"))
    {
        matrixTempCorrection = json["matrixTempCorrection"].as<char *>();
    }

    if (json.containsKey("ntpServer"))
    {
        ntpServer = json["ntpServer"].as<char *>();
    }

    if (json.containsKey("clockTimeZone"))
    {
        clockTimeZone = json["clockTimeZone"].as<float>();
    }

    if (json.containsKey("clockColor"))
    {
        HEXtoRGB(json["clockColor"].as<String>(), clockColorR, clockColorG, clockColorB);
    }

    if (json.containsKey("clockSwitchAktiv"))
    {
        clockSwitchAktiv = json["clockSwitchAktiv"].as<bool>();
    }

    if (json.containsKey("clockSwitchSec"))
    {
        clockSwitchSec = json["clockSwitchSec"].as<uint>();
    }

    if (json.containsKey("clock24Hours"))
    {
        clock24Hours = json["clock24Hours"].as<bool>();
    }

    if (json.containsKey("clockDayLightSaving"))
    {
        clockDayLightSaving = json["clockDayLightSaving"].as<bool>();
    }

    if (json.containsKey("clockWithSeconds"))
    {
        clockWithSeconds = json["clockWithSeconds"].as<bool>();
    }

    if (json.containsKey("clockBlinkAnimated"))
    {
        clockBlinkAnimated = json["clockBlinkAnimated"].as<bool>();
    }

    if (json.containsKey("clockAutoFallbackActive"))
    {
        clockAutoFallbackActive = json["clockAutoFallbackActive"].as<bool>();
    }

    if (json.containsKey("clockAutoFallbackAnimation"))
    {
        clockAutoFallbackAnimation = json["clockAutoFallbackAnimation"].as<uint>();
    }

    if (json.containsKey("clockAutoFallbackTime"))
    {
        clockAutoFallbackTime = json["clockAutoFallbackTime"].as<uint>();
    }

    if (json.containsKey("clockDateDayMonth"))
    {
        clockDateDayMonth = json["clockDateDayMonth"].as<bool>();
    }

    if (json.containsKey("clockDayOfWeekFirstMonday"))
    {
        clockDayOfWeekFirstMonday = json["clockDayOfWeekFirstMonday"].as<bool>();
    }

    if (json.containsKey("clockLargeFont"))
    {
        clockLargeFont = json["clockLargeFont"].as<bool>();
    }

    if (json.containsKey("clockFatFont"))
    {
        clockFatFont = json["clockFatFont"].as<bool>();
    }
    if (json.containsKey("clockDrawWeekDays"))
    {
        clockDrawWeekDays = json["clockDrawWeekDays"].as<bool>();
    }

    if (json.containsKey("scrollTextDefaultDelay"))
    {
        scrollTextDefaultDelay = json["scrollTextDefaultDelay"].as<uint>();
    }

    if (json.containsKey("bootScreenAktiv"))
    {
        bootScreenAktiv = json["bootScreenAktiv"].as<bool>();
    }

    if (json.containsKey("bootBatteryScreen"))
    {
        bootBatteryScreen = json["bootBatteryScreen"].as<bool>();
    }

    if (json.containsKey("bootSound"))
    {
        bootSound = json["bootSound"].as<bool>();
    }

    if (json.containsKey("mqttAktiv"))
    {
        mqttAktiv = json["mqttAktiv"].as<bool>();
    }

    if (json.containsKey("mqttUser"))
    {
        mqttUser = json["mqttUser"].as<char *>();
    }

    if (json.containsKey("mqttPassword"))
    {
        mqttPassword = json["mqttPassword"].as<char *>();
    }

    if (json.containsKey("mqttServer"))
    {
        mqttServer = json["mqttServer"].as<char *>();
    }

    if (json.containsKey("mqttMasterTopic"))
    {
        mqttMasterTopic = json["mqttMasterTopic"].as<char *>();
        mqttMasterTopic.trim();
        if (!mqttMasterTopic.endsWith("/"))
        {
            mqttMasterTopic += "/";
        }
    }

    if (json.containsKey("mqttPort"))
    {
        mqttPort = json["mqttPort"].as<int>();
    }

    if (json.containsKey("mqttUseDeviceTopic"))
    {
        mqttUseDeviceTopic = json["mqttUseDeviceTopic"].as<bool>();
    }

    if (json.containsKey("mqttHAdiscoverable"))
    {
        mqttHAdiscoverable = json["mqttHAdiscoverable"].as<bool>();
    }

    if (json.containsKey("luxOffset"))
    {
        luxOffset = json["luxOffset"].as<float>();
    }

    if (json.containsKey("temperatureOffset"))
    {
        temperatureOffset = json["temperatureOffset"].as<float>();
    }

    if (json.containsKey("humidityOffset"))
    {
        humidityOffset = json["humidityOffset"].as<float>();
    }

    if (json.containsKey("pressureOffset"))
    {
        pressureOffset = json["pressureOffset"].as<float>();
    }

    if (json.containsKey("gasOffset"))
    {
        gasOffset = json["gasOffset"].as<float>();
    }

    if (json.containsKey("dfpRXpin"))
    {
        dfpRXPin = json["dfpRXpin"].as<char *>();
    }

    if (json.containsKey("dfpTXpin"))
    {
        dfpTXPin = json["dfpTXpin"].as<char *>();
    }

    if (json.containsKey("onewirePin"))
    {
        onewirePin = json["onewirePin"].as<char *>();
    }

    if (json.containsKey("SCLPin"))
    {
        SCLPin = json["SCLPin"].as<char *>();
    }

    if (json.containsKey("SDAPin"))
    {
        SDAPin = json["SDAPin"].as<char *>();
    }

    for (uint b = 0; b < 3; b++)
    {
        if (json.containsKey("btn" + String(b) + "Pin"))
        {
            btnPin[b] = json["btn" + String(b) + "Pin"].as<char *>();
        }
        if (json.containsKey("btn" + String(b) + "PressedLevel"))
        {
            btnPressedLevel[b] = json["btn" + String(b) + "PressedLevel"].as<int>();
        }
        if (json.containsKey("btn" + String(b) + "Enabled"))
        {
            btnEnabled[b] = json["btn" + String(b) + "Enabled"].as<bool>();
        }
        if (json.containsKey("btn" + String(b) + "Action"))
        {
            btnAction[b] = static_cast<btnActions>(json["btn" + String(b) + "Action"].as<int>());
        }
    }

    if (json.containsKey("ldrDevice"))
    {
        ldrDevice = json["ldrDevice"].as<char *>();
    }

    if (json.containsKey("ldrPulldown"))
    {
        ldrPulldown = json["ldrPulldown"].as<unsigned long>();
    }

    if (json.containsKey("ldrSmoothing"))
    {
        ldrSmoothing = json["ldrSmoothing"].as<uint>();
    }

    if (json.containsKey("initialVolume"))
    {
        initialVolume = json["initialVolume"].as<uint>();
    }

    if (json.containsKey("sendTelemetry"))
    {
        sendTelemetry = json["sendTelemetry"].as<bool>();
    }

    if (json.containsKey("checkUpdateScreen"))
    {
        checkUpdateScreen = json["checkUpdateScreen"].as<bool>();
    }
}

void EraseWifiCredentials()
{
    wifiManager.resetSettings();
    delay(300);
    ESP.restart();
    delay(300);
}


String GetConfig()
{
#if defined(ESP8266)
    File configFile = LittleFS.open("/config.json", "r");
#elif defined(ESP32)
    File configFile = SPIFFS.open("/config.json", "r");
#endif

    if (configFile)
    {
        // Parse the stream directly: the old file-sized char buffer had no
        // null terminator and could be read past its allocation on WebUI connect.
        DynamicJsonBuffer jsonBuffer;
        JsonObject &root = jsonBuffer.parseObject(configFile);
        if (!root.success())
        {
            Log(F("GetConfig"), F("Invalid config or insufficient memory"));
            return "{}";
        }

        // add current vbat pin to show button on webinterface or not
        root["showBatteryBtn"] = VBAT_PIN > 0 ? true : false;

        String json;
        root.printTo(json);

        return json;
    }
    return "";
}


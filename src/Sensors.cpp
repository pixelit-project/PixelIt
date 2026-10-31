#include "FirmwareState.h"
#include "Tools.h"

String GetSensor()
{
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    if (tempSensor == TempSensor_BME280)
    {
        const float currentTemp = bme280->readTemperature();
        root["temperature"] = currentTemp + temperatureOffset;
        root["humidity"] = bme280->readHumidity() + humidityOffset;
        root["pressure"] = (bme280->readPressure() / 100.0F) + pressureOffset;
        root["gas"] = "Not installed";

        if (temperatureUnit == TemperatureUnit_Fahrenheit)
        {
            root["temperature"] = CelsiusToFahrenheit(currentTemp) + temperatureOffset;
        }
    }
    else if (tempSensor == TempSensor_DHT)
    {
        const float currentTemp = dht.getTemperature();
        root["temperature"] = currentTemp + temperatureOffset;
        root["humidity"] = roundf(dht.getHumidity() + humidityOffset);
        root["pressure"] = "Not installed";
        root["gas"] = "Not installed";

        if (temperatureUnit == TemperatureUnit_Fahrenheit)
        {
            root["temperature"] = CelsiusToFahrenheit(currentTemp) + temperatureOffset;
        }
    }
    else if (tempSensor == TempSensor_BME680)
    {
        /***************************************************************************************************
        // BME680 requires about 100ms for a read (heating the gas sensor). A blocking read can hinder
        // animations and scrolling. Therefore, we will use asynchronous reading in most cases.
        //
        // First call: starts measuring sequence, returns previous values.
        // Second call: performs read, returns current values.
        //
        // As long as there are more than ~200ms between the calls, there won't be blocking.
        // PixelIt usually uses a 3000ms loop.
        //
        // When there's no loop (no Websock connection, no MQTT) but only HTTP API calls, this would result
        // in only EVERY OTHER call return new values (which have been taken shortly after the previous call).
        // This is okay when you are polling very frequently, but might be undesirable when polling every
        // couple of minutes or so. Therefore: if previous reading is more than 20000ms old, perform
        // read in any case, even if it might become blocking.
        //
        // Please note: the gas value not only depends on gas, but also on the time since last read.
        // Frequent reads will yield higher values than infrequent reads. There will be a difference
        // even if we switch from 6secs to 3secs! So, do not attempt to compare values of readings
        // with an interval of 3 secs to values of readings with an interval of 60 secs!
        */

        const int elapsedSinceLastRead = millis() - lastBME680read;
        const int remain = bme680->remainingReadingMillis();

        if (remain == -1) // no current values available
        {
            bme680->beginReading(); // start measurement process
            // return previous values
            const float currentTemp = bme680->temperature;
            root["temperature"] = currentTemp + temperatureOffset;
            root["humidity"] = bme680->humidity + humidityOffset;
            root["pressure"] = (bme680->pressure / 100.0F) + pressureOffset;
            root["gas"] = (bme680->gas_resistance / 1000.0F) + gasOffset;
            if (temperatureUnit == TemperatureUnit_Fahrenheit)
            {
                root["temperature"] = CelsiusToFahrenheit(currentTemp) + temperatureOffset;
            }
        }

        if (remain >= 0 || elapsedSinceLastRead > 20000)
        // remain==0: measurement completed, not read yet
        // remain>0: measurement still running, but as we already are in the next loop call, block and read
        // elapsedSinceLastRead>20000: obviously, remain==-1. But as there haven't been loop calls recently, this seems to be an "infrequent" API call. Perform blocking read.
        {
            if (bme680->endReading()) // will become blocking if measurement not complete yet
            {
                lastBME680read = millis();
                const float currentTemp = bme680->temperature;
                root["temperature"] = currentTemp + temperatureOffset;
                root["humidity"] = bme680->humidity + humidityOffset;
                root["pressure"] = (bme680->pressure / 100.0F) + pressureOffset;
                root["gas"] = (bme680->gas_resistance / 1000.0F) + gasOffset;
                if (temperatureUnit == TemperatureUnit_Fahrenheit)
                {
                    root["temperature"] = CelsiusToFahrenheit(currentTemp) + temperatureOffset;
                }
            }
            else
            {
                root["humidity"] = "Error while reading";
                root["temperature"] = "Error while reading";
                root["pressure"] = "Error while reading";
                root["gas"] = "Error while reading";
            }
        }
    }
    else if (tempSensor == TempSensor_BMP280)
    {
        const float currentTemp = bmp280->readTemperature();
        root["temperature"] = currentTemp + temperatureOffset;
        // root["humidity"] = bmp280->readHumidity() + humidityOffset;
        root["humidity"] = "Not installed";
        root["pressure"] = (bmp280->readPressure() / 100.0F) + pressureOffset;
        root["gas"] = "Not installed";

        if (temperatureUnit == TemperatureUnit_Fahrenheit)
        {
            root["temperature"] = CelsiusToFahrenheit(currentTemp) + temperatureOffset;
        }
    }
    else if (tempSensor == TempSensor_SHT31)
    {
        const float currentTemp = sht31.readTemperature();
        const float currentHumi = sht31.readHumidity();
        root["temperature"] = currentTemp + temperatureOffset;
        root["humidity"] = roundf(currentHumi + humidityOffset);
        root["pressure"] = "Not installed";
        root["gas"] = "Not installed";
    }
    else
    {
        root["humidity"] = "Not installed";
        root["temperature"] = "Not installed";
        root["pressure"] = "Not installed";
        root["gas"] = "Not installed";
    }

    if (VBAT_PIN > 0)
    {
        root["battery"] = batteryLevel;
    }
    else
    {
        root["battery"] = "Not installed";
    }
    root["hostname"] = hostname;

    String json;
    root.printTo(json);

    // Log(F("Sensor readings"), F("Hum/Temp/Press/Gas:"));
    // Log(F("Sensor readings"), json);
    return json;
}

String GetLuxSensor()
{
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    root["lux"] = currentLux;
    root["hostname"] = hostname;

    String json;
    root.printTo(json);

    return json;
}

String GetBrightness()
{
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    root["brightness_255"] = currentMatrixBrightness;
    root["brightness"] = map(currentMatrixBrightness, 0, 255, 0, 100);
    root["hostname"] = hostname;

    String json;
    root.printTo(json);

    return json;
}

String GetMatrixInfo()
{
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    root["pixelitVersion"] = VERSION;
    root["latestReleaseVersion"] = compareVersions(lastReleaseVersion.c_str(), VERSION) > 0 ? lastReleaseVersion : String(VERSION);
    root["updateCheckCompleted"] = firmwareUpdateChecked;
    //// Matrix Config
    root["note"] = note;
    root["hostname"] = hostname;
    root["deviceID"] = deviceID;
    root["buildSection"] = STR(BUILD_SECTION);
    root["freeSketchSpace"] = ESP.getFreeSketchSpace();
    root["wifiRSSI"] = WiFi.RSSI();
    root["wifiQuality"] = GetRSSIasQuality(WiFi.RSSI());
    root["wifiSSID"] = WiFi.SSID();
    root["ipAddress"] = WiFi.localIP().toString();
    root["freeHeap"] = ESP.getFreeHeap();
    root["currentMatrixBrightness"] = currentMatrixBrightness;
    root["wifiBSSID"] = WiFi.BSSIDstr();

#if defined(ESP8266)
    root["sketchSize"] = ESP.getSketchSize();
    root["chipID"] = ESP.getChipId();
    root["maxFreeBlock"] = ESP.getMaxFreeBlockSize();
    root["heapFragmentation"] = ESP.getHeapFragmentation();
    root["resetInfo"] = ESP.getResetInfo();
#elif defined(ESP32)
    root["chipID"] = uint64ToString(ESP.getEfuseMac());
#endif

    root["cpuFreqMHz"] = ESP.getCpuFreqMHz();
    root["sleepMode"] = sleepMode;
    root["matrixBrightnessAutomatic"] = matrixBrightnessAutomatic;
    root["clockAutoFallbackActive"] = clockAutoFallbackActive;
    root["clockAutoFallbackTime"] = clockAutoFallbackTime;
    root["displayText"] = haDisplayText;
    root["textMessage"] = haTextMessage;
    root["textColor"] = haTextColor;
    root["textFont"] = haTextFont;
    root["textAlignment"] = haTextAlignment;
    root["textScroll"] = haTextScroll;
    root["uptime"] = millis() / 1000;
    root["resetReason"] = ResetReason();
    root["animationFrames"] = animationBmpCount;
    root["maxAnimationFrames"] = MAX_ANIMATION_FRAMES;
    root["maxAnimationPixels"] = MAX_ANIMATION_PIXELS;
    root["maxBitmapPixels"] = MAX_BITMAP_PIXELS;
    root["mqttBufferSize"] = client.getBufferSize();
    root["mqttConnected"] = client.connected();
    root["mqttState"] = client.state();
    JsonObject &matrix = root.createNestedObject("matrixsize");
    matrix["cols"] = MATRIX_WIDTH;
    matrix["rows"] = MATRIX_HEIGHT;

    String json;
    root.printTo(json);

    return json;
}

String GetButtons()
{
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    for (uint button = 0; button < 3; button++)
    {
        root[btnAPINames[button]] = btnLastPublishState[button] ? "true" : "false";
    }
    root["hostname"] = hostname;

    String json;
    root.printTo(json);

    return json;
}

void SendTelemetry()
{
    Log(F("SendTelemetry"), F("Sending..."));
    HttpClient httpClient = HttpClient(wifiClientHTTP, TELEMETRY_SERVER_HOST, TELEMETRY_SERVER_PORT);
    httpClient.setTimeout(1500);
    httpClient.sendHeader("User-Agent", "PixelIt");
    httpClient.post(TELEMETRY_SERVER_PATH, "application/json", GetTelemetry());
}

String GetTelemetry()
{
    const String MatrixTypeNames[] = {"Colum major", "Row major", "Tiled 4x 8x8 CJMCU (Column major)", "MicroMatrix", "Tiled 4x 8x8 CJMCU (Row major)"};
    const String TempSensorNames[] = {"none", "BME280", "DHT", "BME680", "BMP280", "SHT31"};
    const String LuxSensorNames[] = {"LDR", "BH1750", "Max44009"};

    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    root["uuid"] = sha1(GetChipID());
    root["version"] = VERSION;
    root["type"] = isESP8266 ? "esp8266" : "esp32";
    root["buildSection"] = STR(BUILD_SECTION);

    JsonObject &matrix = root.createNestedObject("matrix");
    matrix["type"] = matrixType;
    matrix["name"] = MatrixTypeNames[matrixType - 1];
    matrix["width"] = MATRIX_WIDTH;
    matrix["height"] = MATRIX_HEIGHT;

    JsonArray &sensors = root.createNestedArray("sensors");
    sensors.add(LuxSensorNames[luxSensor]);
    if (tempSensor > 0)
    {
        sensors.add(TempSensorNames[tempSensor]);
    }

    String json;
    root.printTo(json);
    return json;
}


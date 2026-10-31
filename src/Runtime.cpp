#include "FirmwareState.h"
#include "Tools.h"
#include "UpdateScreen.h"

void setup()
{
#if defined(ULANZI)
    pinMode(15, INPUT_PULLDOWN); // Fix high pitch tone
    pinMode(27, INPUT_PULLUP);   // Middle Button fix
    pinMode(26, INPUT_PULLUP);   // Left Button fix
    pinMode(VBAT_PIN, INPUT);    // Battery ADC
#endif

    Serial.begin(115200);

    // Mounting FileSystem
    Serial.println(F("Mounting file system..."));
#if defined(ESP8266)
    if (LittleFS.begin())
#elif defined(ESP32)
    if (SPIFFS.begin(true))
#endif
    {
        Serial.println(F("Mounted file system."));
        LoadConfig();
        // If new version detected, create new variables in config if necessary.
        if (optionsVersion != VERSION)
        {
            Log(F("LoadConfig"), F("New version detected, create new variables in config if necessary"));
            SaveConfig();
            LoadConfig();
        }
    }
    else
    {
        Serial.println(F("Failed to mount FS"));
    }

    // Init SetGPIO Array
    for (int i = 0; i < SET_GPIO_SIZE; i++)
    {
        setGPIOReset[i].gpio = -1;
        setGPIOReset[i].resetMillis = -1;
    }

    // I2C Sensors
    twowire.begin(TranslatePin(SDAPin), TranslatePin(SCLPin));

    // Init LightSensor
    bh1750 = new BH1750();
    if (bh1750->begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23, &twowire))
    {
        Log(F("Setup"), F("BH1750 started"));
        luxSensor = LuxSensor_BH1750;
    }
    else
    {
        delete bh1750;
        max44009 = new Max44009(MAX44009_DEFAULT_ADDRESS, &twowire);
        if (max44009->isConnected())
        {
            Log(F("Setup"), F("Max44009/GY-049 started"));
            luxSensor = LuxSensor_Max44009;
        }
        else
        {
            delete max44009;
            photocell = new LightDependentResistor(LDR_PIN, ldrPulldown, TranslatePhotocell(ldrDevice), 10, ldrSmoothing);
            photocell->setPhotocellPositionOnGround(false);
            luxSensor = LuxSensor_LDR;
        }
    }

    // Init Temp Sensors
    Log(F("Setup"), F("SHT31 Trying"));
    if (sht31.begin(0x44))
    {
        Log(F("Setup"), F("SHT31 started"));
        tempSensor = TempSensor_SHT31;
    }
    else
    {
        Log(F("Setup"), F("BME280 Trying"));
        bme280 = new Adafruit_BME280();
        if (bme280->begin(BME280_ADDRESS_ALTERNATE, &twowire))
        {
            Log(F("Setup"), F("BME280 started"));
            tempSensor = TempSensor_BME280;
        }
        else
        {
            delete bme280;
            bmp280 = new Adafruit_BMP280(&twowire);
            Log(F("Setup"), F("BMP280 Trying"));
            if (bmp280->begin(BMP280_ADDRESS_ALT, 0x58))
            {
                Log(F("Setup"), F("BMP280 started"));
                tempSensor = TempSensor_BMP280;
            }
            else
            {
                delete bmp280;
                bme680 = new Adafruit_BME680(&twowire);
                Log(F("Setup"), F("BME680 Trying"));
                if (bme680->begin())
                {
                    Log(F("Setup"), F("BME680 started"));
                    tempSensor = TempSensor_BME680;
                }
                else
                {
                    Log(F("Setup"), F("No SHT31, BMP280, BME280 or BME680 sensor found"));
                    // AM2320 needs a delay to be reliably initialized
                    delete bme680;

                    // continue only if:
                    //  - LDR is being used. This means: no light sensor in I²C bus.
                    //  - SDA and SCL use different pin than onewire

                    // Otherwise, we already found a light sensor on I²C. If we would start a probe for OneWire on the same pin now, I²C will be disfunctional.
                    if (luxSensor == LuxSensor_LDR || (onewirePin != SDAPin && onewirePin != SCLPin))
                    {
                        delay(800);
                        dht.setup(TranslatePin(onewirePin), DHTesp::DHT22);
                        Log(F("Setup"), F("DHT Trying"));
                        if (!isnan(dht.getHumidity()) && !isnan(dht.getTemperature()))
                        {
                            Log(F("Setup"), F("DHT started"));
                            tempSensor = TempSensor_DHT;
                        }
                        else
                        {
                            Log(F("Setup"), F("No DHT Sensor found"));
                        }
                    }
                    else
                    {
                        Log(F("Setup"), F("Not probing DHT sensor: light sensor already found on same pin as DHT."));
                    }
                }
            }
        }
    }

    switch (matrixType)
    {
    default: // Matix Type 1 (Colum major)
        matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG);
        break;

    case 2: // Matix Type 2 (Row major)
        matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG);
        break;

    case 3: // Matix Type 3 (Tiled 4x 8x8 CJMCU, Column major)
        matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_BOTTOM + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS + NEO_MATRIX_PROGRESSIVE);
        break;

    case 4: // Matix Type 4 (MicroMatrix by foorschtbar) See: https://github.com/foorschtbar/Sk6805EC15-Matrix
        matrix = new FastLED_NeoMatrix(leds, 8, 8, 4, 1, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG);
        break;

    case 5: // Matix Type 5 (Tiled 4x 8x8 CJMCU, Row major)
        matrix = new FastLED_NeoMatrix(leds, 8, 8, 4, 1, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_PROGRESSIVE);
        break;
    }

    ColorTemperature userColorTemp = GetUserColorTemp();
    LEDColorCorrection userLEDCorrection = GetUserColorCorrection();

    // Matrix Color Correction
    if (userLEDCorrection != UncorrectedColor)
    {
        FastLED.addLeds<NEOPIXEL, MATRIX_PIN>(leds, MATRIX_WIDTH * MATRIX_HEIGHT).setCorrection(userLEDCorrection);
    }
    else if (userColorTemp != UncorrectedTemperature)
    {
        FastLED.addLeds<NEOPIXEL, MATRIX_PIN>(leds, MATRIX_WIDTH * MATRIX_HEIGHT).setTemperature(userColorTemp);
    }
    else
    {
        int *rgbArray = GetUserCutomCorrection();
        FastLED.addLeds<NEOPIXEL, MATRIX_PIN>(leds, MATRIX_WIDTH * MATRIX_HEIGHT).setCorrection(matrix->Color(rgbArray[0], rgbArray[1], rgbArray[2]));
    }

    matrix->begin();
    matrix->setTextWrap(false);
    matrix->setBrightness(currentMatrixBrightness);
    matrix->clear();

    softSerial = new SoftwareSerial(TranslatePin(dfpTXPin), TranslatePin(dfpRXPin));

    softSerial->begin(9600);
    Log(F("Setup"), F("Software Serial started"));

    // Play sound on boot
    if (bootSound)
    {
        delay(1000); // is needed for the dfplayer to startup
        initDFPlayer();
        delay(10);
        mp3Player.play(1);
    }

    // Bootscreen
    if (bootScreenAktiv)
    {
        ShowBootAnimation();
    }

    // Battery
    if (bootBatteryScreen)
    {
        ShowBatteryScreen();
    }

    // Set unique device ID
    deviceID = "PixelIt-";
#if defined(ESP8266)
    deviceID += ESP.getChipId();
#elif defined(ESP32)
    deviceID += uint64ToString(ESP.getEfuseMac());
#endif
    // Set hostname from config
    // variable is already validated in LoadConfig()
    if (hostname.isEmpty())
    {
        hostname = deviceID;
    }
    WiFi.hostname(hostname);
    mqttDeviceTopic = mqttMasterTopic + hostname + "/";

    ShowConnectingScreen();

    wifiManager.setAPCallback(EnteredHotspotCallback);
    wifiManager.setMinimumSignalQuality();
    // Timout for the wifi connection until the hotspot is set up
    wifiManager.setConnectTimeout(30);
    // Config menue timeout 180 seconds.
    wifiManager.setConfigPortalTimeout(180);

    if (!wifiManager.autoConnect("PIXELIT"))
    {
        Log(F("Setup"), F("Wifi failed to connect and hit timeout"));
        connectingAnimTicker.detach();
        delay(3000);
        // Reset and try again, or maybe put it to deep sleep
        ESP.restart();
        delay(5000);
    }
    connectingAnimTicker.detach();
#if defined(ESP8266)
    // Avoid modem-sleep wakeup delays in network yields during playback.
    WiFi.setSleepMode(WIFI_NONE_SLEEP);
#endif

    Log(F("Setup"), F("Wifi connected...yeey :)"));

    Log(F("Setup"), F("Local IP"));
    Log(F("Setup"), WiFi.localIP().toString());
    Log(F("Setup"), WiFi.gatewayIP().toString());
    Log(F("Setup"), WiFi.subnetMask().toString());

    Log(F("Setup"), F("Starting UDP"));
    udp.begin(2390);
    // Log(F("Setup"), "Local port: " + String(udp.localPort()));

    httpUpdater.setup(&server);

    server.on(F("/api/screen"), HTTP_POST, HandleScreen);
    server.on(F("/api/luxsensor"), HTTP_GET, HandleGetLuxSensor);
    server.on(F("/api/brightness"), HTTP_GET, HandleGetBrightness);
    server.on(F("/api/dhtsensor"), HTTP_GET, HandleGetDHTSensor); // Legacy
    server.on(F("/api/sensor"), HTTP_GET, HandleGetSensor);
    server.on(F("/api/buttons"), HTTP_GET, HandleGetButtons);
    server.on(F("/api/matrixinfo"), HTTP_GET, HandleGetMatrixInfo);
    server.on(F("/api/animationinfo"), HTTP_GET, HandleGetAnimationInfo);
    // server.on(F("/api/soundinfo"), HTTP_GET, HandleGetSoundInfo);
    server.on(F("/api/config"), HTTP_POST, HandleSetConfig);
    server.on(F("/api/config"), HTTP_GET, HandleGetConfig);
    server.on(F("/api/wifireset"), HTTP_POST, HandelWifiConfigReset);
    server.on(F("/api/factoryreset"), HTTP_POST, HandleFactoryReset);
    server.on(F("/"), HTTP_GET, HandleGetMainPage);
    server.onNotFound(HandleNotFound);

    server.begin();

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    // Liveview
    liveview.begin(matrix, leds, SEND_LIVEVIEW_INTERVAL); // pass pointer to matrix, ledbuffer and interval
    liveview.setCallback(sendLiveview);                   // set callback function which is called after the interval

    Log(F("Setup"), F("Webserver started"));

    if (mqttAktiv == true)
    {
        client.setServer(mqttServer.c_str(), mqttPort);
        client.setCallback(callback);
#if defined(ESP8266)
        const uint16_t initialMqttBufferSize = MQTT_IDLE_BUFFER_SIZE;
#else
        const uint16_t initialMqttBufferSize = MQTT_BUFFER_SIZE;
#endif
        if (!client.setBufferSize(initialMqttBufferSize))
        {
            Log(F("Setup"), F("Failed to allocate MQTT buffer"));
        }
        Log(F("Setup"), F("MQTT started"));
    }

    if (!bootSound)
    {
        initDFPlayer();
    }
}

void displayUpdateScreen()
{
    Log(F("UpdateScreen"), F("Display UpdateScreen..."));

    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    BuildUpdateScreenJSON(root);

    if (root.success())
    {
        CreateFrames(root, CHECKUPDATESCREEN_DURATION);
    }
    else
    {
        Log(F("UpdateScreen"), F("Failed to Build UpdateScreen JSON"));
    }
}

void checkUpdate()
{
    Log(F("CheckUpdate"), F("Checking..."));
    HttpClient httpClient = HttpClient(wifiClientHTTP, CHECKUPDATE_SERVER_HOST, CHECKUPDATE_SERVER_PORT);
    httpClient.sendHeader("User-Agent", "PixelIt");
    httpClient.setTimeout(1500);
    httpClient.setHttpResponseTimeout(1500);
    httpClient.get(CHECKUPDATE_SERVER_PATH);
    int statusCode = httpClient.responseStatusCode();
    // A failed status read must not trigger another wait for headers/body.
    String response;
    if (statusCode == 200)
        response = httpClient.responseBody();
    // Version checks are hours apart; release the TCP buffers immediately.
    httpClient.stop();

    // Serial.print("Status code: ");
    // Serial.println(statusCode);
    // Serial.print("Response: ");
    // Serial.println(response);

    if (statusCode == 200)
    {
        DynamicJsonBuffer jsonBuffer;
        JsonObject &root = jsonBuffer.parseObject(response);
        if (root.success() && root["version"].is<const char *>() && root["version"].as<String>().length() > 0)
        {
            lastReleaseVersion = root["version"].as<String>();
            firmwareUpdateChecked = true;

            int result = compareVersions(lastReleaseVersion.c_str(), VERSION);

            if (result > 0)
            {
                Log(F("CheckUpdate"), "New FW available " + String(VERSION) + " -> " + lastReleaseVersion);
            }
            else
            {
                Log(F("CheckUpdate"), "No new FW available " + String(VERSION) + " -> " + lastReleaseVersion);
            }
            SendMatrixInfo();
        }
    }
    else
    {
        Log(F("CheckUpdate"), "Error. HTTP Code: " + String(statusCode));
    }
}

void RecordAnimationWork(const char *task, unsigned long started)
{
    const unsigned long elapsed = millis() - started;
    if (animateBMPAktivLoop && elapsed > animationSlowestTaskMillis)
    {
        animationSlowestTaskMillis = elapsed;
        animationSlowestTask = task;
    }
}

void loop()
{
    // MQTT retains only its small idle buffer while HTTP/WebSocket run.
    {
        const unsigned long started = millis();
        server.handleClient();
        RecordAnimationWork("HTTP", started);
    }
    {
        const unsigned long started = millis();
        webSocket.loop();
        RecordAnimationWork("WebSocket", started);
    }

    // Update Battery level
    if (millis() - batteryLevelPrevMillis >= UPDATE_BATTERY_LEVEL_INTERVAL)
    {
        batteryLevelPrevMillis = millis();
        getBatteryVoltage();
    }

    // Reset GPIO based on the array, as far as something is present in the array.
    for (int i = 0; i < SET_GPIO_SIZE; i++)
    {
        if (setGPIOReset[i].gpio != -1)
        {
            if (setGPIOReset[i].resetMillis <= millis())
            {
                Log(F("ResetSetGPIO"), "Pos: " + String(i) + ", GPIO: " + String(setGPIOReset[i].gpio) + ", Value: false");
                digitalWrite(setGPIOReset[i].gpio, false);
                setGPIOReset[i].gpio = -1;
                setGPIOReset[i].resetMillis = -1;
            }
        }
    }

    // Check and display if new FW version is available.
    // if necessary also check scrollTextAktivLoop = false; and animateBMPAktivLoop = false; if they are disturbed?!
    if (checkUpdateScreen || (mqttAktiv && mqttHAdiscoverable))
    {

        // Defer blocking internet requests while pixels or text are moving.
        // Check new FW Version first time after 30.5 seconds
        if (!animateBMPAktivLoop && !scrollTextAktivLoop &&
            ((checkUpdatePrevMillis == 0 && millis() > 30500) || millis() - checkUpdatePrevMillis >= CHECKUPDATE_INTERVAL))
        {
            checkUpdatePrevMillis = millis();
            checkUpdate();
        }

        // Display new FW Version
        if (checkUpdateScreen && millis() - checkUpdateScreenPrevMillis >= CHECKUPDATESCREEN_INTERVAL)
        {
            checkUpdateScreenPrevMillis = millis();

            int result = compareVersions(lastReleaseVersion.c_str(), VERSION);
            if (result > 0)
            {
                if (!sleepMode)
                {
                    displayUpdateScreen();
                }
            }
        }
    }

    // Send Telemetry data first time after 30.3 seconds
    // if necessary also check scrollTextAktivLoop = false; and animateBMPAktivLoop = false; if they are disturbed?!
    if (sendTelemetry == true && !animateBMPAktivLoop && !scrollTextAktivLoop && ((sendTelemetryPrevMillis == 0 && millis() > 30300) || millis() - sendTelemetryPrevMillis >= SEND_TELEMETRY_INTERVAL))
    {
        sendTelemetryPrevMillis = millis();
        SendTelemetry();
    }

    if (mqttAktiv == true)
    {
        const unsigned long nowMillis = millis();
        const bool connected = client.connected();
        const bool reconnectDue = !connected && (mqttLastReconnectAttempt == 0 || nowMillis - mqttLastReconnectAttempt >= MQTT_RECONNECT_INTERVAL);
        const bool receiveDue = connected && (wifiClientMQTT.available() > 0 || nowMillis - mqttLastService >= 1000 || HADiscoveryDue());
        // Resize only before entering PubSubClient. Its read functions retain
        // pointers into the buffer, so resizing from Client::available is unsafe.
        if ((reconnectDue || receiveDue) && PrepareMqttServiceBuffer())
        {
            mqttLastService = nowMillis;
            if (reconnectDue)
            {
                mqttLastReconnectAttempt = nowMillis;
                if (MQTTreconnect())
                    mqttLastReconnectAttempt = 0;
            }
            else
            {
                {
                    const unsigned long started = millis();
                    client.loop();
                    ServiceHADiscovery();
                    RecordAnimationWork("MQTT", started);
                }
            }
#if defined(ESP8266)
            client.setBufferSize(MQTT_IDLE_BUFFER_SIZE);
#endif
            // Persist config only after releasing the large receive buffer.
            ServiceMqttConfig();
            ServiceHAText();
        }
    }

    ServiceButtons();

    // Clock Auto Fallback
    if (!sleepMode && ((clockAutoFallbackActive && !clockAktiv && millis() - lastScreenMessageMillis >= (clockAutoFallbackTime * 1000)) || forceClock))
    {
        forceClock = false;
        scrollTextAktivLoop = false;
        animateBMPAktivLoop = false;

        int performWipe = 0;

        switch (clockAutoFallbackAnimation)
        {
        case 1:
        case 2:
        case 3:
            performWipe = clockAutoFallbackAnimation;
            break;
        case 4:
            performWipe = (millis() % 3) + 1;
            break;
        default:;
        }

        if (performWipe == 1)
        {
            FadeOut();
        }
        else if (performWipe == 2)
        {
            ColoredBarWipe();
        }
        else if (performWipe == 3)
        {
            ZigZagWipe(clockColorR, clockColorG, clockColorB);
        }
        clockAktiv = true;
        clockCounterClock = 0;
        clockCounterDate = 0;
        DrawClock(true);

        if (performWipe != 0)
        {
            FadeIn();
        }
    }

    if (clockAktiv && now() != clockLastUpdate)
    {
        if (timeStatus() == timeNotSet && ntpTimeOut <= millis())
        {
            if (ntpRetryCounter >= NTP_MAX_RETRYS)
            {
                ntpTimeOut = (millis() + (NTP_TIMEOUT_SEC * 1000));
                ntpRetryCounter = 0;
                Log(F("Sync TimeServer"), "sync timeout for " + String(NTP_TIMEOUT_SEC) + " seconds!");
            }
            else
            {
                Log(F("Sync TimeServer"), ntpServer + " waiting for sync");
                setSyncProvider(getNtpTime);
            }
        }
        clockLastUpdate = now();
        DrawClock(false);
    }

    // Get Lunx and control brightness
    if (millis() - getLuxPrevMillis >= SEND_LUX_INTERVAL)
    {
        getLuxPrevMillis = millis();

        if (luxSensor == LuxSensor_BH1750)
        {
            currentLux = bh1750->readLightLevel() + luxOffset;
        }
        else if (luxSensor == LuxSensor_Max44009)
        {
            currentLux = max44009->getLux() + luxOffset;
        }
        else
        {
            currentLux = (roundf(photocell->getSmoothedLux() * 1000) / 1000) + luxOffset;
        }

        if (!sleepMode && matrixBrightnessAutomatic)
        {
            float newBrightness = map(currentLux, mbaLuxMin, mbaLuxMax, mbaDimMin, mbaDimMax);
            // Max brightness 255
            if (newBrightness > 255)
            {
                newBrightness = 255;
            }
            // Min brightness 0
            if (newBrightness < 0)
            {
                newBrightness = 0;
            }

            if (newBrightness != currentMatrixBrightness)
            {
                SetCurrentMatrixBrightness(newBrightness);
                Log(F("Auto Brightness"), "Lux: " + String(currentLux) + " set brightness to " + String(currentMatrixBrightness));
                matrix->show();
            }
        }
    }

    // Send LDR values non-foreced
    if (millis() - sendLuxPrevMillis >= SEND_LUX_INTERVAL)
    {
        sendLuxPrevMillis = millis();
        {
            const unsigned long started = millis();
            SendLDR(false);
            RecordAnimationWork("LuxPublish", started);
        }
    }

    // Send Sensor values non-foreced
    if (millis() - sendSensorPrevMillis >= SEND_SENSOR_INTERVAL)
    {
        sendSensorPrevMillis = millis();
        {
            const unsigned long started = millis();
            SendSensor(false);
            RecordAnimationWork("Sensor", started);
        }
    }

    // liveview
    {
        const unsigned long started = millis();
        liveview.loop(animateBMPAktivLoop ? animateBMPDelay : 0);
        RecordAnimationWork("Liveview", started);
    }

    // send matrix info
    if (millis() - sendInfoPrevMillis >= SEND_MATRIXINFO_INTERVAL)
    {
        sendInfoPrevMillis = millis();
        {
            const unsigned long started = millis();
            SendMatrixInfo();
            RecordAnimationWork("MatrixInfo", started);
        }
        // SendMp3PlayerInfo(false);
    }

    if (!sleepMode && (animateBMPAktivLoop && millis() - animateBMPPrevMillis >= animateBMPDelay))
    {
        animateBMPPrevMillis = millis();
        {
            const unsigned long started = millis();
            AnimateBMP(true);
            RecordAnimationWork("LED", started);
        }
    }

    if (!sleepMode && (scrollTextAktivLoop && millis() - scrollTextPrevMillis >= scrollTextDelay))
    {
        scrollTextPrevMillis = millis();
        ScrollText(false);
    }
}


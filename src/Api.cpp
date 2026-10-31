#include "FirmwareState.h"
#include "Tools.h"
#include "Webinterface.h"

void HandleGetMainPage()
{
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("text/html"), mainPage);
}

void HandleNotFound()
{
    if (server.method() == HTTP_OPTIONS)
    {
        server.sendHeader(F("Access-Control-Allow-Origin"), "*");
        server.send(204);
    }

    server.sendHeader("Location", "/update", true);
    server.send(302, F("text/plain"), "");
}

bool PublishMqtt(const char *topic, const char *payload, bool retained)
{
    if (!topic || !client.connected())
        return false;
    const size_t needed = MQTT_MAX_HEADER_SIZE + 2 + strlen(topic) + (payload ? strlen(payload) : 0);
    if (needed > MQTT_BUFFER_SIZE)
        return false;
    const uint16_t previousSize = client.getBufferSize();
    if (needed > previousSize && !client.setBufferSize(static_cast<uint16_t>(needed)))
        return false;
    const bool result = client.publish(topic, payload, retained);
    if (client.getBufferSize() != previousSize)
        client.setBufferSize(previousSize);
    return result;
}

bool PrepareBitmapStorage()
{
    if (!bmpArray)
        bmpArray.reset(new (std::nothrow) uint16_t[MAX_BITMAP_PIXELS]());
    return static_cast<bool>(bmpArray);
}

bool PrepareBitmapAnimation(JsonObject &screen)
{
    if (!IsValidBitmapAnimation(screen))
        return false;
    if (screen.containsKey("bitmapAnimation") && !animationBmpList)
    {
        // Allocate while the incoming packet occupies the large heap block.
        // Keeping pixel storage in the remaining heap avoids splitting the
        // contiguous block needed by subsequent HTTP/MQTT/WebSocket packets.
        animationBmpList.reset(new (std::nothrow) uint16_t[MAX_ANIMATION_PIXELS]);
        if (!animationBmpList)
            return false;
    }
    return !screen.containsKey("bitmapAnimation") || PrepareBitmapStorage();
}

void SendWebSocketJson(uint8_t num, const String &payload)
{
    webSocket.sendTXT(num, payload.c_str(), payload.length());
}

void HandleScreen()
{
    if (!server.hasArg("plain"))
    {
        server.send(400, F("application/json"), F("{\"response\":\"Missing request body\"}"));
        return;
    }
    DynamicJsonBuffer jsonBuffer;
#if defined(ESP8266)
    // The server owns a mutable request String; reuse its buffer to avoid a
    // second full copy of the animation in the ESP8266's limited heap.
    String &args = const_cast<String &>(server.arg("plain"));
    struct ReleaseRequestBody
    {
        String &body;
        // Assigning an empty String retains capacity on this ESP8266 core.
        // A null C string invalidates it and actually releases the POST buffer.
        ~ReleaseRequestBody() { body = static_cast<const char *>(nullptr); }
    } releaseBody{args};
    char *payload = args.begin();
#else
    String args = server.arg("plain");
    char *payload = args.begin();
#endif
    if (!PackAnimationData(payload, args.length()))
    {
        server.send(406, F("application/json"), F("{\"response\":\"Invalid animation data\"}"));
        return;
    }
    JsonObject &json = jsonBuffer.parseObject(payload);
    server.sendHeader(F("Connection"), F("close"));
    server.sendHeader(F("Access-Control-Allow-Origin"), "*");

    if (json.success() && PrepareBitmapAnimation(json))
    {
        Log(F("HandleScreen"), "Incoming JSON length: " + String(json.measureLength()));
        CreateFrames(json);
        // Release the large input before acknowledging the command, so the
        // sender cannot overlap its next request with this retained buffer.
        args = static_cast<const char *>(nullptr);
        server.send(200, F("application/json"), F("{\"response\":\"OK\"}"));
    }
    else
    {
        server.send(406, F("application/json"), F("{\"response\":\"Not Acceptable\"}"));
    }
}

void HandleSetConfig()
{
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.parseObject(server.arg("plain"));
    server.sendHeader(F("Connection"), F("close"));

    if (json.success())
    {
        Log(F("SetConfig"), "Incoming JSON length: " + String(json.measureLength()));
        SetConfig(json);
        server.send(200, F("application/json"), F("{\"response\":\"OK\"}"));
        delay(500);
        ESP.restart();
    }
    else
    {
        server.send(406, F("application/json"), F("{\"response\":\"Not Acceptable\"}"));
    }
}

void HandleGetConfig()
{
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("application/json"), GetConfig());
}

void HandleGetLuxSensor()
{
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("application/json"), GetLuxSensor());
}

void HandleGetBrightness()
{
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("application/json"), GetBrightness());
}

void HandleGetDHTSensor() // Legancy
{
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("application/json"), GetSensor());
}

void HandleGetSensor()
{
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("application/json"), GetSensor());
}

void HandleGetButtons()
{
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("application/json"), GetButtons());
}

void HandleGetAnimationInfo()
{
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    root["animationDelay"] = animateBMPDelay;
    root["animationRenderedFrames"] = animationRenderedFrames;
    root["animationMaxFrameGap"] = animationMaxFrameGap;
    root["animationFailedLedFrames"] = animationFailedLedFrames;
    root["slowestTask"] = animationSlowestTask;
    root["slowestTaskMillis"] = animationSlowestTaskMillis;
#if defined(ESP8266)
    root["ledFramesSent"] = _frame_cnt;
    root["ledFrameRetries"] = _retry_cnt;
#endif
    String json;
    root.printTo(json);
    server.send(200, F("application/json"), json);
}

void HandleGetMatrixInfo()
{
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("application/json"), GetMatrixInfo());
}

void HandelWifiConfigReset()
{
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("application/json"), F("{\"response\":\"OK\"}"));
    EraseWifiCredentials();
}

void HandleFactoryReset()
{
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("application/json"), F("{\"response\":\"OK\"}"));
#if defined(ESP8266)
    File configFile = LittleFS.open("/config.json", "w");
#elif defined(ESP32)
    File configFile = SPIFFS.open("/config.json", "w");
#endif
    if (!configFile)
    {
        Log(F("Handle_factoryreset"), F("Failed to open config file for reset"));
    }
    configFile.println("");
    configFile.close();
    EraseWifiCredentials();
}

void SleepScreen(bool startSleep, bool forceClockOnWake)
{
    if (startSleep)
    {
        Log(F("SleepScreen"), F("Sleeping..."));
        matrix->clear();
        DrawTextHelper("z", false, false, false, false, false, 0, 0, 255, (MATRIX_WIDTH / 2) - 6, 1);
        matrix->show();
        delay(200);
        DrawTextHelper("Z", false, false, false, false, false, 0, 0, 255, (MATRIX_WIDTH / 2) - 1, 1);
        matrix->show();
        delay(200);
        DrawTextHelper("z", false, false, false, false, false, 0, 0, 255, (MATRIX_WIDTH / 2) + 4, 1);
        matrix->show();
        delay(500);
        FadeOut(30, 0);
        matrix->setBrightness(0);
        matrix->show();
    }
    else
    {
        Log(F("SleepScreen"), F("Waking up..."));
        matrix->clear();
        // DrawTextHelper("😀", false, true, false, false, false, 255, 200, 0, 0, 1);
        // FadeIn(30, 0);
        // delay(150);
        forceClock = forceClockOnWake;
    }
}

void HandleAndSendButtonPress(uint button, bool state)
{
    btnLastPublishState[button] = state;
    PublishButtonEvent(button, state ? "press" : "release");
    Log(F("Buttons"), btnLogNames[button] + " is now " + (state ? "true" : "false"));

    // Prüfen ob über MQTT versendet werden muss
    if (mqttAktiv == true && client.connected())
    {
        PublishMqtt((mqttMasterTopic + "buttons/button" + String(button)).c_str(), (state ? "true" : "false"), true);
        if (mqttUseDeviceTopic)
        {
            PublishMqtt((mqttDeviceTopic + "buttons/button" + String(button)).c_str(), (state ? "true" : "false"), true);
        }
    }
    // Prüfen ob über Websocket versendet werden muss
    if (webSocket.connectedClients() > 0)
    {
        for (uint i = 0; i < sizeof websocketConnection / sizeof websocketConnection[0]; i++)
        {
            SendWebSocketJson(i, "{\"buttons\":{\"" + btnAPINames[button] + "\":" + (state ? "true" : "false") + "}}");
        }
    }

    if (state == false)
    {
        return;
    }

    if (btnAction[button] == btnAction_ToggleSleepMode)
    {
        sleepMode = !sleepMode;
        SleepScreen(sleepMode, true);
    }
    if (btnAction[button] == btnAction_GotoClock)
    {
        forceClock = true;
    }
    if (btnAction[button] == btnAction_MP3PlayPrevious)
    {
        mp3Player.playPrevious();
    }
    if (btnAction[button] == btnAction_MP3PlayNext)
    {
        mp3Player.playNext();
    }
    if (btnAction[button] == btnAction_MP3PlayPause)
    {
        if (mp3Player.isPlaying())
        {
            mp3Player.pause();
        }
        else
        {
            delay(200);
            mp3Player.resume();
        }
    }
}

void callback(char *topic, byte *payload, unsigned int length)
{
    if (String(topic) == "homeassistant/status")
    {
        if (length == 6 && memcmp(payload, "online", 6) == 0) RequestHADiscovery();
        return;
    }
    if (QueueHATextCommand(topic, payload, length)) return;
    if (length > 0 && payload[0] == '{')
    {
        payload[length] = '\0';
        if (!PackAnimationData(reinterpret_cast<char *>(payload), length))
        {
            Log(F("MQTT_callback"), F("Invalid animation data"));
            return;
        }
        String channel = String(topic);
        int lastSlashIndex = channel.lastIndexOf('/');
        if (lastSlashIndex != -1)
        {
            channel = channel.substring(lastSlashIndex + 1);
        }

        DynamicJsonBuffer jsonBuffer;
        JsonObject &json = jsonBuffer.parseObject(payload);

        Log("MQTT_callback", "Incoming JSON (Topic: " + String(topic) + ", Cmd: " + channel + ", Bytes: " + String(length) + "/" + String(json.measureLength()) + ") ");

        if (!json.success() || json.measureLength() == 2)
        {
            Log("MQTT_callback", "JSON message empty or too long");
            return;
        }
        if (channel.equals("setScreen"))
        {
            CreateFrames(json);
            SendMatrixInfo();
        }
        else if (channel.equals("getLuxsensor"))
        {
            PublishMqtt((mqttMasterTopic + "luxsensor").c_str(), GetLuxSensor().c_str());
            if (mqttUseDeviceTopic)
            {
                PublishMqtt((mqttDeviceTopic + "luxsensor").c_str(), GetLuxSensor().c_str());
            }
        }
        else if (channel.equals("getMatrixinfo"))
        {
            PublishMqtt((mqttMasterTopic + "matrixinfo").c_str(), GetMatrixInfo().c_str());
            if (mqttUseDeviceTopic)
            {
                PublishMqtt((mqttDeviceTopic + "matrixinfo").c_str(), GetMatrixInfo().c_str());
            }
        }
        else if (channel.equals("getConfig"))
        {
            PublishMqtt((mqttMasterTopic + "config").c_str(), GetConfig().c_str());
            if (mqttUseDeviceTopic)
            {
                PublishMqtt((mqttDeviceTopic + "config").c_str(), GetConfig().c_str());
            }
        }
        else if (channel.equals("setConfig"))
        {
            QueueMqttConfig(json);
        }
    }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
    switch (type)
    {
    case WStype_DISCONNECTED:
    {
        webSocket.resetPreview(num);
        Log("WebSocketEvent", "[" + String(num) + "] Disconnected!");
        websocketConnection[num] = "";
        break;
    }
    case WStype_CONNECTED:
    {
        webSocket.resetPreview(num);
        // Remember for what the connection was established
        websocketConnection[num] = String((char *)payload);

        // get ip
        IPAddress ip = webSocket.remoteIP(num);

        // Logging
        Log(F("WebSocketEvent"), "[" + String(num) + "] Connected from " + ip.toString() + " url: " + websocketConnection[num]);

        // send message to client
        SendMatrixInfo();
        SendLDR(true);
        SendSensor(true);
        SendConfig();
        SendWebSocketJson(num, "{\"buttons\":" + GetButtons() + "}");
        SendWebSocketJson(num, "{\"telemetry\":" + GetTelemetry() + "}");
        break;
    }
    case WStype_TEXT:
    {
        if (length > 0)
        {
            size_t first = 0;
            while (first < length && AnimationWhitespace(static_cast<char>(payload[first])))
                first++;
            // The WebUI also sends non-JSON heartbeats. Preserve the original
            // behavior of ignoring them, while accepting whitespace before JSON.
            if (first == length || payload[first] != '{')
                break;
            if (length > MAX_SCREEN_MESSAGE_SIZE || !PackAnimationData(reinterpret_cast<char *>(payload), length))
            {
                webSocket.sendTXT(num, "{\"response\":\"Invalid or oversized animation data\"}");
                return;
            }
            DynamicJsonBuffer jsonBuffer;
            JsonObject &json = jsonBuffer.parseObject(payload);
            int forcedDuration = 0;

            // Logging
            Log(F("WebSocketEvent"), "Incoming JSON (Length: " + String(length) + "/" + String(json.measureLength()) + ")");
            if (!json.success())
            {
                Log(F("WebSocketEvent"), F("Invalid JSON or insufficient memory"));
                webSocket.sendTXT(num, "{\"response\":\"Invalid JSON or insufficient memory\"}");
                return;
            }

            if (json.containsKey("forcedDuration"))
            {
                forcedDuration = json["forcedDuration"].as<int>();
            }

            if (json.containsKey("setScreen"))
            {
                JsonObject &screen = json["setScreen"].as<JsonObject>();
                if (!screen.success() || !PrepareBitmapAnimation(screen))
                {
                    webSocket.sendTXT(num, "{\"response\":\"Invalid animation: maximum 28 frames, 256 pixels per frame, 1792 pixels total\"}");
                    return;
                }
                CreateFrames(screen, forcedDuration);
                webSocket.sendTXT(num, "{\"response\":\"OK\"}");
            }
            else if (json.containsKey("setConfig"))
            {
                SetConfig(json["setConfig"]);
                delay(500);
                ESP.restart();
            }
            else if (json.containsKey("wifiReset"))
            {
                if (json["wifiReset"].as<bool>() == true)
                {
                    HandelWifiConfigReset();
                }
            }
            else if (json.containsKey("factoryReset"))
            {
                if (json["factoryReset"].as<bool>() == true)
                {
                    HandleFactoryReset();
                }
            }
            else if (json.containsKey("sendTelemetry"))
            {
                sendTelemetryPrevMillis = 0;
            }
        }
        break;
    }
    case WStype_BIN:
        break;
    case WStype_FRAGMENT_BIN_START:
        break;
    case WStype_FRAGMENT_TEXT_START:
        break;
    case WStype_FRAGMENT:
        break;
    case WStype_FRAGMENT_FIN:
        break;
    case WStype_PING:
        break;
    case WStype_PONG:
        break;
    case WStype_ERROR:
        break;
    }
}


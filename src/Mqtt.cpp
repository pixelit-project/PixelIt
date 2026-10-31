#include "FirmwareState.h"
#include "Tools.h"

boolean MQTTreconnect()
{

    bool connected = false;
    if (mqttUser != NULL && mqttUser.length() > 0 && mqttPassword != NULL && mqttPassword.length() > 0)
    {
        Log(F("MQTTreconnect"), F("MQTT connecting to broker..."));
        connected = client.connect(hostname.c_str(), mqttUser.c_str(), mqttPassword.c_str(), ((mqttUseDeviceTopic ? mqttDeviceTopic : mqttMasterTopic) + "state").c_str(), 0, true, "disconnected");
    }
    else
    {
        Log(F("MQTTreconnect"), F("MQTT connecting to broker..."));
        connected = client.connect(hostname.c_str(), ((mqttUseDeviceTopic ? mqttDeviceTopic : mqttMasterTopic) + "state").c_str(), 0, true, "disconnected");
    }

    if (connected)
    {
        Log(F("MQTTreconnect"), F("MQTT connected!"));
        // Subscribe to topics ...
        client.subscribe((mqttMasterTopic + "setScreen").c_str());
        client.subscribe((mqttMasterTopic + "setText").c_str());
        client.subscribe((mqttMasterTopic + "setTextOptions").c_str());
        client.subscribe((mqttMasterTopic + "setTextOptions/#").c_str());
        client.subscribe((mqttMasterTopic + "showText").c_str());
        client.subscribe((mqttMasterTopic + "getLuxsensor").c_str());
        client.subscribe((mqttMasterTopic + "getMatrixinfo").c_str());
        client.subscribe((mqttMasterTopic + "getConfig").c_str());
        client.subscribe((mqttMasterTopic + "setConfig").c_str());
        if (mqttUseDeviceTopic)
        {
            client.subscribe((mqttDeviceTopic + "setScreen").c_str());
            client.subscribe((mqttDeviceTopic + "setText").c_str());
            client.subscribe((mqttDeviceTopic + "setTextOptions").c_str());
            client.subscribe((mqttDeviceTopic + "setTextOptions/#").c_str());
            client.subscribe((mqttDeviceTopic + "showText").c_str());
            client.subscribe((mqttDeviceTopic + "getLuxsensor").c_str());
            client.subscribe((mqttDeviceTopic + "getMatrixinfo").c_str());
            client.subscribe((mqttDeviceTopic + "getConfig").c_str());
            client.subscribe((mqttDeviceTopic + "setConfig").c_str());
        }
        // ... and publish state ....
        PublishMqtt((mqttMasterTopic + "state").c_str(), "connected", true);
        if (mqttUseDeviceTopic)
        {
            PublishMqtt((mqttDeviceTopic + "state").c_str(), "connected", true);
        }

        client.subscribe("homeassistant/status");
        RequestHADiscovery();
        ServiceHADiscovery();
    }
    else
    {
        Log(F("MQTTreconnect"), F("MQTT connect failed! Retry in a few seconds..."));
    }

    return connected;
}
namespace
{
bool haDiscoveryPending = false;
String pendingMqttConfig;
uint8_t haDiscoveryStep = 0;
unsigned long haDiscoveryLastStep = 0;

struct HASensor
{
    const char *id;
    const char *name;
    const char *value;
    const char *unit;
    const char *deviceClass;
};
const HASensor haSensors[] = {
    {"Temperature", "Temperature", "temperature", "°C", "temperature"},
    {"Humidity", "Humidity", "humidity", "%", "humidity"},
    {"Pressure", "Pressure", "pressure", "hPa", "pressure"},
    {"VOC", "Gas resistance", "gas", "kOhm", ""},
    {"Illuminance", "Illuminance", "lux", "lx", "illuminance"},
    {"wifiRSSI", "Wifi Signal", "wifiRSSI", "dBm", "signal_strength"},
    {"WifiQuality", "Wifi Quality", "wifiQuality", "%", ""},
    {"cpuFreqMHz", "CPU Freq.", "cpuFreqMHz", "MHz", ""},
    {"WifiSSID", "SSID", "wifiSSID", "", ""},
    {"WifiBSSID", "BSSID", "wifiBSSID", "", ""},
    {"chipID", "Chip ID", "chipID", "", ""},
    {"Uptime", "Uptime", "uptime", "s", "duration"},
    {"FreeHeap", "Free memory", "freeHeap", "B", ""},
    {"ResetReason", "Restart reason", "resetReason", "", ""},
};
const uint8_t haSensorCount = sizeof(haSensors) / sizeof(haSensors[0]);
const uint8_t haControlCount = 12;
const uint8_t haConfigCount = haSensorCount + haControlCount + 6;

// Build and send only one entity at a time. Keep the established discovery
// topics and unique IDs so existing HA entity customizations survive upgrades.
void PublishHAConfig(uint8_t step)
{
    const String base = mqttUseDeviceTopic ? mqttDeviceTopic : mqttMasterTopic;
    DynamicJsonBuffer buffer;
    JsonObject &root = buffer.createObject();
    String component, id, name;
    bool enabled = mqttHAdiscoverable;
    if (step < haSensorCount)
    {
        const HASensor &sensor = haSensors[step];
        component = "sensor";
        id = sensor.id;
        name = sensor.name;
        if (step == 0) enabled &= tempSensor != TempSensor_None;
        if (step == 1) enabled &= tempSensor != TempSensor_None && tempSensor != TempSensor_BMP280;
        if (step == 2) enabled &= tempSensor == TempSensor_BME280 || tempSensor == TempSensor_BMP280 || tempSensor == TempSensor_BME680;
        if (step == 3) enabled &= tempSensor == TempSensor_BME680;
        root["state_topic"] = base + (step < 4 ? "sensor" : step == 4 ? "luxsensor" : "matrixinfo");
        root["value_template"] = "{{ value_json." + String(sensor.value) + " }}";
        if (sensor.unit[0]) root["unit_of_measurement"] = step == 0 && temperatureUnit == TemperatureUnit_Fahrenheit ? "°F" : sensor.unit;
        if (sensor.deviceClass[0]) root["device_class"] = sensor.deviceClass;
        if (step >= 5)
        {
            root["entity_category"] = "diagnostic";
            root["enabled_by_default"] = false;
        }
        if (step == 11 || step == 12) root["state_class"] = "measurement";
    }
    else if (step < haSensorCount + haControlCount)
    {
        const uint8_t control = step - haSensorCount;
        root["state_topic"] = base + "matrixinfo";
        if (control == 0)
        {
            component = "light";
            id = "LEDMatrixLight";
            name = "LED Matrix";
            root["schema"] = "template";
            root["state_template"] = F("{{ 'on' if value_json.sleepMode is false else 'off' }}");
            root["command_topic"] = base + "setScreen";
            root["command_on_template"] = F("{\"sleepMode\": false {%- if brightness is defined -%}, \"brightness\": {{ brightness }}{%- endif -%}}");
            root["command_off_template"] = F("{\"sleepMode\": true}");
            root["brightness_template"] = F("{{ value_json.currentMatrixBrightness }}");
            root["icon"] = "mdi:led-strip";
        }
        else if (control == 1 || control == 2)
        {
            component = "switch";
            const bool brightness = control == 1;
            id = brightness ? "AutoBrightness" : "ClockAutoFallback";
            name = brightness ? "Automatic brightness" : "Automatic clock";
            root["command_topic"] = base + (brightness ? "setScreen" : "setConfig");
            root["value_template"] = brightness ? F("{{ 'ON' if value_json.matrixBrightnessAutomatic else 'OFF' }}") : F("{{ 'ON' if value_json.clockAutoFallbackActive else 'OFF' }}");
            root["payload_on"] = brightness ? "{\"autobrightness\":true}" : "{\"clockAutoFallbackActive\":true}";
            root["payload_off"] = brightness ? "{\"autobrightness\":false}" : "{\"clockAutoFallbackActive\":false}";
            root["state_on"] = "ON";
            root["state_off"] = "OFF";
            root["icon"] = brightness ? "mdi:brightness-auto" : "mdi:clock-outline";
            if (!brightness) root["entity_category"] = "config";
        }
        else if (control == 3)
        {
            component = "number";
            id = "ClockAutoFallbackTime";
            name = "Clock fallback delay";
            root["value_template"] = F("{{ value_json.clockAutoFallbackTime }}");
            root["command_topic"] = base + "setConfig";
            root["command_template"] = F("{\"clockAutoFallbackTime\": {{ value | int }}}");
            root["min"] = 1;
            root["max"] = 86400;
            root["step"] = 1;
            root["mode"] = "box";
            root["unit_of_measurement"] = "s";
            root["entity_category"] = "config";
        }
        else if (control == 4)
        {
            component = "text";
            id = "DisplayText";
            name = "Display text";
            root["command_topic"] = base + "setTextOptions/text";
            root["value_template"] = F("{{ value_json.textMessage }}");
            root["min"] = 0;
            root["max"] = 255;
            root["icon"] = "mdi:message-text";
        }
        else if (control == 5)
        {
            component = "text";
            id = "TextColor";
            name = "Text color";
            root["command_topic"] = base + "setTextOptions/color";
            root["value_template"] = F("{{ value_json.textColor }}");
            root["min"] = 7;
            root["max"] = 7;
            root["pattern"] = "^#[0-9A-Fa-f]{6}$";
            root["icon"] = "mdi:palette";
        }
        else if (control >= 6 && control <= 8)
        {
            component = "select";
            root["command_topic"] = base + "setTextOptions/" + (control == 6 ? "font" : control == 7 ? "alignment" : "scroll");
            JsonArray &options = root.createNestedArray("options");
            if (control == 6)
            {
                id = "TextFont"; name = "Text font size";
                root["value_template"] = F("{{ value_json.textFont }}");
                options.add("Small"); options.add("Large");
                root["icon"] = "mdi:format-size";
            }
            else if (control == 7)
            {
                id = "TextAlignment"; name = "Text alignment";
                root["value_template"] = F("{{ value_json.textAlignment }}");
                options.add("Centered"); options.add("Left");
                root["icon"] = "mdi:format-align-center";
            }
            else
            {
                id = "TextScroll"; name = "Text scrolling";
                root["value_template"] = F("{{ value_json.textScroll }}");
                options.add("Auto"); options.add("Always"); options.add("Off");
                root["icon"] = "mdi:arrow-left-right";
            }
        }
        else if (control == 9)
        {
            component = "button";
            id = "ShowText";
            name = "Show text";
            root.remove("state_topic");
            root["command_topic"] = base + "showText";
            root["payload_press"] = "PRESS";
            root["icon"] = "mdi:message-arrow-right";
        }
        else if (control == 10)
        {
            component = "light";
            id = "TextColorPicker";
            name = "Text color picker";
            root["schema"] = "template";
            root["command_topic"] = base + "setTextOptions/colorPicker";
            root["command_on_template"] = F("{% if red is defined and green is defined and blue is defined %}#{{ '%02X%02X%02X' | format(red, green, blue) }}{% else %}ON{% endif %}");
            root["command_off_template"] = "OFF";
            root["state_template"] = F("{{ 'off' if value_json.textColor == '#000000' else 'on' }}");
            root["red_template"] = F("{{ value_json.textColor[1:3] | int(base=16) }}");
            root["green_template"] = F("{{ value_json.textColor[3:5] | int(base=16) }}");
            root["blue_template"] = F("{{ value_json.textColor[5:7] | int(base=16) }}");
            root["icon"] = "mdi:palette";
        }
        else
        {
            component = "update";
            id = "FirmwareUpdate";
            name = "Firmware";
            root["device_class"] = "firmware";
            root["entity_category"] = "config";
            root["title"] = "PixelIt firmware";
            root["value_template"] = F("{{ value_json.pixelitVersion }}");
            root["latest_version_topic"] = base + "matrixinfo";
            root["latest_version_template"] = F("{{ value_json.latestReleaseVersion }}");
            root["release_url"] = "https://github.com/pixelit-project/PixelIt/releases/latest";
        }
    }
    else
    {
        const uint8_t button = (step - haSensorCount - haControlCount) / 2;
        const bool event = (step - haSensorCount - haControlCount) % 2;
        enabled &= btnEnabled[button];
        component = event ? "event" : "binary_sensor";
        id = "Button" + String(button) + (event ? "Event" : "");
        name = String(btnLogNames[button]) + (event ? " event" : "");
        root["state_topic"] = base + "buttons/button" + String(button) + (event ? "/event" : "");
        root["icon"] = "mdi:gesture-tap-button";
        if (event)
        {
            JsonArray &types = root.createNestedArray("event_types");
            types.add("press"); types.add("release"); types.add("single_press");
            types.add("double_press"); types.add("long_press");
        }
        else
        {
            root["payload_on"] = "true";
            root["payload_off"] = "false";
        }
    }
    const String topic = "homeassistant/" + component + "/" + deviceID + "/" + deviceID + id + "/config";
    if (!enabled)
    {
        PublishMqtt(topic.c_str(), "", true);
        return;
    }
    JsonObject &device = root.createNestedObject("device");
    device["identifiers"] = deviceID;
    device["name"] = hostname;
    device["manufacturer"] = "PixelIt";
    device["model"] = "PixelIt";
    device["sw_version"] = VERSION;
    device["configuration_url"] = "http://" + WiFi.localIP().toString();
    root["unique_id"] = deviceID + id;
    root["name"] = name;
    root["availability_topic"] = base + "state";
    root["payload_available"] = "connected";
    root["payload_not_available"] = "disconnected";
    String payload;
    root.printTo(payload);
    PublishMqtt(topic.c_str(), payload.c_str(), true);
}
}

void RequestHADiscovery()
{
    haDiscoveryPending = true;
    haDiscoveryStep = 0;
}

bool HADiscoveryDue()
{
    if (!haDiscoveryPending || millis() - haDiscoveryLastStep < 40) return false;
    // Leave enough time for a single MQTT write before the next LED frame.
    // Near the deadline, let playback run first. Slow-frame animations still
    // make progress immediately after each rendered frame.
    if (animateBMPAktivLoop && animationRenderedFrames > 0)
    {
        const unsigned long elapsed = millis() - animateBMPPrevMillis;
        const unsigned long budget = animateBMPDelay < 100 ? animateBMPDelay / 2 : 80;
        if (elapsed >= animateBMPDelay || animateBMPDelay - elapsed < budget) return false;
    }
    return true;
}

void ServiceHADiscovery()
{
    if (!client.connected() || !HADiscoveryDue()) return;
    haDiscoveryLastStep = millis();
    if (haDiscoveryStep < haConfigCount)
    {
        PublishHAConfig(haDiscoveryStep);
    }
    else
    {
        const String base = mqttUseDeviceTopic ? mqttDeviceTopic : mqttMasterTopic;
        const uint8_t state = haDiscoveryStep - haConfigCount;
        // Initial states are also spread out; avoid a second burst at the end.
        if (state == 0) PublishMqtt((base + "matrixinfo").c_str(), GetMatrixInfo().c_str(), true);
        else if (state == 1) PublishMqtt((base + "sensor").c_str(), GetSensor().c_str(), true);
        else if (state == 2) PublishMqtt((base + "luxsensor").c_str(), GetLuxSensor().c_str(), true);
        else if (btnEnabled[state - 3]) PublishMqtt((base + "buttons/button" + String(state - 3)).c_str(), btnLastPublishState[state - 3] ? "true" : "false", true);
    }
    if (++haDiscoveryStep >= haConfigCount + 6) haDiscoveryPending = false;
}

void QueueMqttConfig(JsonObject &config)
{
    pendingMqttConfig = "";
    config.printTo(pendingMqttConfig);
}

void ServiceMqttConfig()
{
    if (pendingMqttConfig.length() == 0) return;
    DynamicJsonBuffer buffer;
    JsonObject &config = buffer.parseObject(pendingMqttConfig);
    if (config.success()) SetConfig(config);
    pendingMqttConfig = "";
}

bool PrepareMqttServiceBuffer()
{
    // A fragmented ESP8266 heap may not have an 11 KB contiguous block while
    // playing an animation. Keep small HA controls and MQTT keepalive working;
    // the full animation buffer is still attempted first on every service.
    return client.setBufferSize(MQTT_BUFFER_SIZE) || client.setBufferSize(2048);
}

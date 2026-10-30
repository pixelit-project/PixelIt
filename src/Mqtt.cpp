#include "FirmwareState.h"
#include "Tools.h"

boolean MQTTreconnect()
{

    bool connected = false;
    if (mqttUser != NULL && mqttUser.length() > 0 && mqttPassword != NULL && mqttPassword.length() > 0)
    {
        Log(F("MQTTreconnect"), F("MQTT connecting to broker..."));
        connected = client.connect(hostname.c_str(), mqttUser.c_str(), mqttPassword.c_str(), (mqttMasterTopic + "state").c_str(), 0, true, "disconnected");
    }
    else
    {
        Log(F("MQTTreconnect"), F("MQTT connecting to broker..."));
        connected = client.connect(hostname.c_str(), (mqttMasterTopic + "state").c_str(), 0, true, "disconnected");
    }

    if (connected)
    {
        Log(F("MQTTreconnect"), F("MQTT connected!"));
        // Subscribe to topics ...
        client.subscribe((mqttMasterTopic + "setScreen").c_str());
        client.subscribe((mqttMasterTopic + "getLuxsensor").c_str());
        client.subscribe((mqttMasterTopic + "getMatrixinfo").c_str());
        client.subscribe((mqttMasterTopic + "getConfig").c_str());
        client.subscribe((mqttMasterTopic + "setConfig").c_str());
        if (mqttUseDeviceTopic)
        {
            client.subscribe((mqttDeviceTopic + "setScreen").c_str());
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

        // ... and provide discovery information
        // Create discovery information for Homeassistant
        // Can also be processed by ioBroker, OpenHAB etc.
        if (mqttHAdiscoverable)
        {

            // Get host IP to provide URL in MQTT discovery device info
            String ip_url = "http://" + WiFi.localIP().toString();
            String configTopicTemplate = String(F("homeassistant/#COMPONENT#/#DEVICEID#/#DEVICEID##SENSORID#/config"));
            configTopicTemplate.replace(F("#DEVICEID#"), deviceID);
            String configPayloadTemplate = String(F(
                "{"
                "\"dev\":{"
                "\"ids\":\"#DEVICEID#\","
                "\"name\":\"#HOSTNAME#\","
                "\"mdl\":\"PixelIt\","
                "\"mf\":\"PixelIt\","
                "\"sw\":\"#VERSION#\","
                "\"cu\":\"#IP#\""
                "},"
                "\"avty_t\":\"#MASTERTOPIC#state\","
                "\"pl_avail\":\"connected\","
                "\"pl_not_avail\":\"disconnected\","
                "\"uniq_id\":\"#DEVICEID##SENSORID#\","
                "\"dev_cla\":\"#CLASS#\","
                "\"name\":\"#SENSORNAME#\","
                "\"stat_t\":\"#MASTERTOPIC##STATETOPIC#\","
                "\"unit_of_meas\":\"#UNIT#\","
                "\"val_tpl\":\"{{value_json.#VALUENAME#}}\""
                "}"));
            configPayloadTemplate.replace(" ", "");
            configPayloadTemplate.replace(F("#DEVICEID#"), deviceID);
            configPayloadTemplate.replace(F("#HOSTNAME#"), hostname);
            configPayloadTemplate.replace(F("#VERSION#"), VERSION);
            if (mqttUseDeviceTopic)
            {
                configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttDeviceTopic);
            }
            else
            {
                configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttMasterTopic);
            }
            configPayloadTemplate.replace(F("#IP#"), ip_url);

            String topic;
            String payload;

            if (tempSensor != TempSensor_None)
            {
                topic = configTopicTemplate;
                topic.replace(F("#COMPONENT#"), F("sensor"));
                topic.replace(F("#SENSORID#"), F("Temperature"));

                payload = configPayloadTemplate;
                payload.replace(F("#SENSORID#"), F("Temperature"));
                payload.replace(F("#SENSORNAME#"), F("Temperature"));
                payload.replace(F("#CLASS#"), F("temperature"));
                payload.replace(F("#STATETOPIC#"), F("sensor"));
                payload.replace(F("#UNIT#"), "°C");
                payload.replace(F("#VALUENAME#"), F("temperature"));
                PublishMqtt(topic.c_str(), payload.c_str(), true);

                topic = configTopicTemplate;
                topic.replace(F("#COMPONENT#"), F("sensor"));
                topic.replace(F("#SENSORID#"), F("Humidity"));

                payload = configPayloadTemplate;
                payload.replace(F("#SENSORNAME#"), F("Humidity"));
                payload.replace(F("#CLASS#"), F("humidity"));
                payload.replace(F("#STATETOPIC#"), F("sensor"));
                payload.replace(F("#UNIT#"), "%");
                payload.replace(F("#VALUENAME#"), F("humidity"));
                PublishMqtt(topic.c_str(), payload.c_str(), true);
            }
            if (tempSensor == TempSensor_BME280 || tempSensor == TempSensor_BMP280 || tempSensor == TempSensor_BME680)
            {
                topic = configTopicTemplate;
                topic.replace(F("#COMPONENT#"), F("sensor"));
                topic.replace(F("#SENSORID#"), F("Pressure"));

                payload = configPayloadTemplate;
                payload.replace(F("#SENSORID#"), F("Pressure"));
                payload.replace(F("#SENSORNAME#"), F("Pressure"));
                payload.replace(F("#CLASS#"), F("pressure"));
                payload.replace(F("#STATETOPIC#"), F("sensor"));
                payload.replace(F("#UNIT#"), "hPa");
                payload.replace(F("#VALUENAME#"), F("pressure"));
                PublishMqtt(topic.c_str(), payload.c_str(), true);
            }

            if (tempSensor == TempSensor_BME680)
            {
                topic = configTopicTemplate;
                topic.replace(F("#COMPONENT#"), F("sensor"));
                topic.replace(F("#SENSORID#"), F("VOC"));

                payload = configPayloadTemplate;
                payload.replace(F("#SENSORID#"), F("VOC"));
                payload.replace(F("#SENSORNAME#"), F("VOC"));
                payload.replace(F("#CLASS#"), F("volatile_organic_compounds"));
                payload.replace(F("#STATETOPIC#"), F("sensor"));
                payload.replace(F("#UNIT#"), "kOhm");
                payload.replace(F("#VALUENAME#"), F("gas"));
                PublishMqtt(topic.c_str(), payload.c_str(), true);
            }
            topic = configTopicTemplate;
            topic.replace(F("#COMPONENT#"), F("sensor"));
            topic.replace(F("#SENSORID#"), F("Illuminance"));

            payload = configPayloadTemplate;
            payload.replace(F("#SENSORID#"), F("Illuminance"));
            payload.replace(F("#SENSORNAME#"), F("Illuminance"));
            payload.replace(F("#CLASS#"), F("illuminance"));
            payload.replace(F("#STATETOPIC#"), F("luxsensor"));
            payload.replace(F("#UNIT#"), "lx");
            payload.replace(F("#VALUENAME#"), F("lux"));
            PublishMqtt(topic.c_str(), payload.c_str(), true);

            configPayloadTemplate = String(F(
                "{"
                "\"dev\":{"
                "\"ids\":\"#DEVICEID#\","
                "\"name\":\"#HOSTNAME#\","
                "\"mdl\":\"PixelIt\","
                "\"mf\":\"PixelIt\","
                "\"sw\":\"#VERSION#\","
                "\"cu\":\"#IP#\""
                "},"
                "\"avty_t\":\"#MASTERTOPIC#state\","
                "\"pl_avail\":\"connected\","
                "\"pl_not_avail\":\"disconnected\","
                "\"uniq_id\":\"#DEVICEID##SENSORID#\","
                "\"name\":\"#SENSORNAME#\","
                "\"ic\":\"mdi:gesture-tap-button\","
                "\"pl_on\":\"true\","
                "\"pl_off\":\"false\","
                "\"stat_t\":\"#MASTERTOPIC##STATETOPIC#\""
                "}"));
            configPayloadTemplate.replace(" ", "");
            configPayloadTemplate.replace(F("#DEVICEID#"), deviceID);
            configPayloadTemplate.replace(F("#HOSTNAME#"), hostname);
            configPayloadTemplate.replace(F("#VERSION#"), VERSION);
            if (mqttUseDeviceTopic)
            {
                configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttDeviceTopic);
            }
            else
            {
                configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttMasterTopic);
            }
            configPayloadTemplate.replace(F("#IP#"), ip_url);

            for (uint8_t n = 0; n < sizeof(btnEnabled) / sizeof(btnEnabled[0]); n++)
            {
                if (btnEnabled[n])
                {
                    topic = configTopicTemplate;
                    topic.replace(F("#COMPONENT#"), F("binary_sensor"));
                    topic.replace(F("#SENSORID#"), String(F("Button")) + String(n));

                    payload = configPayloadTemplate;
                    payload.replace(F("#SENSORID#"), String(F("Button")) + String(n));
                    payload.replace(F("#SENSORNAME#"), String(btnLogNames[n]));
                    payload.replace(F("#STATETOPIC#"), String(F("buttons/button")) + String(n));
                    PublishMqtt(topic.c_str(), payload.c_str(), true);
                }
            }

            // Wifi RSSI
            configPayloadTemplate = String(F(
                "{"
                "\"dev\":{"
                "\"ids\":\"#DEVICEID#\","
                "\"name\":\"#HOSTNAME#\","
                "\"mdl\":\"PixelIt\","
                "\"mf\":\"PixelIt\","
                "\"sw\":\"#VERSION#\","
                "\"cu\":\"#IP#\""
                "},"
                "\"avty_t\":\"#MASTERTOPIC#state\","
                "\"pl_avail\":\"connected\","
                "\"pl_not_avail\":\"disconnected\","
                "\"uniq_id\":\"#DEVICEID##SENSORID#\","
                "\"dev_cla\":\"signal_strength\","
                "\"name\":\"#SENSORNAME#\","
                "\"stat_t\":\"#MASTERTOPIC##STATETOPIC#\","
                "\"unit_of_meas\":\"#UNIT#\","
                "\"val_tpl\":\"{{value_json.#VALUENAME#}}\","
                "\"ent_cat\":\"diagnostic\","
                "\"ic\":\"mdi:#ICON#\","
                "\"enabled_by_default\":\"false\""
                "}"));
            configPayloadTemplate.replace(" ", "");
            configPayloadTemplate.replace(F("#DEVICEID#"), deviceID);
            configPayloadTemplate.replace(F("#HOSTNAME#"), hostname);
            configPayloadTemplate.replace(F("#VERSION#"), VERSION);
            if (mqttUseDeviceTopic)
            {
                configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttDeviceTopic);
            }
            else
            {
                configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttMasterTopic);
            }
            configPayloadTemplate.replace(F("#IP#"), ip_url);

            topic = configTopicTemplate;
            topic.replace(F("#COMPONENT#"), F("sensor"));
            topic.replace(F("#SENSORID#"), F("wifiRSSI"));

            payload = configPayloadTemplate;
            payload.replace(F("#SENSORID#"), F("wifiRSSI"));
            payload.replace(F("#SENSORNAME#"), F("Wifi Signal"));
            payload.replace(F("#CLASS#"), F("signal_strength"));
            payload.replace(F("#STATETOPIC#"), F("matrixinfo"));
            payload.replace(F("#UNIT#"), "dBm");
            payload.replace(F("#VALUENAME#"), F("wifiRSSI"));
            payload.replace(F("#ICON#"), F("signal"));
            PublishMqtt(topic.c_str(), payload.c_str(), true);

            // Wifi Quality sensor
            configPayloadTemplate = String(F(
                "{"
                "\"dev\":{"
                "\"ids\":\"#DEVICEID#\","
                "\"name\":\"#HOSTNAME#\","
                "\"mdl\":\"PixelIt\","
                "\"mf\":\"PixelIt\","
                "\"sw\":\"#VERSION#\","
                "\"cu\":\"#IP#\""
                "},"
                "\"avty_t\":\"#MASTERTOPIC#state\","
                "\"pl_avail\":\"connected\","
                "\"pl_not_avail\":\"disconnected\","
                "\"uniq_id\":\"#DEVICEID##SENSORID#\","
                "\"name\":\"#SENSORNAME#\","
                "\"stat_t\":\"#MASTERTOPIC##STATETOPIC#\","
                "\"unit_of_meas\":\"#UNIT#\","
                "\"val_tpl\":\"{{value_json.#VALUENAME#}}\","
                "\"ent_cat\":\"diagnostic\","
                "\"ic\":\"mdi:#ICON#\","
                "\"enabled_by_default\":\"false\""
                "}"));
            configPayloadTemplate.replace(" ", "");
            configPayloadTemplate.replace(F("#DEVICEID#"), deviceID);
            configPayloadTemplate.replace(F("#HOSTNAME#"), hostname);
            configPayloadTemplate.replace(F("#VERSION#"), VERSION);
            if (mqttUseDeviceTopic)
            {
                configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttDeviceTopic);
            }
            else
            {
                configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttMasterTopic);
            }
            configPayloadTemplate.replace(F("#IP#"), ip_url);

            topic = configTopicTemplate;
            topic.replace(F("#COMPONENT#"), F("sensor"));
            topic.replace(F("#SENSORID#"), F("WifiQuality"));

            payload = configPayloadTemplate;
            payload.replace(F("#SENSORID#"), F("WifiQuality"));
            payload.replace(F("#SENSORNAME#"), F("Wifi Quality"));
            payload.replace(F("#STATETOPIC#"), F("matrixinfo"));
            payload.replace(F("#UNIT#"), "%");
            payload.replace(F("#VALUENAME#"), F("wifiQuality"));
            payload.replace(F("#ICON#"), F("signal"));
            PublishMqtt(topic.c_str(), payload.c_str(), true);

            // CPU Freq.
            topic = configTopicTemplate;
            topic.replace(F("#COMPONENT#"), F("sensor"));
            topic.replace(F("#SENSORID#"), F("cpuFreqMHz"));

            payload = configPayloadTemplate;
            payload.replace(F("#SENSORID#"), F("cpuFreqMHz"));
            payload.replace(F("#SENSORNAME#"), F("CPU Freq."));
            payload.replace(F("#STATETOPIC#"), F("matrixinfo"));
            payload.replace(F("#UNIT#"), "MHz");
            payload.replace(F("#VALUENAME#"), F("cpuFreqMHz"));
            payload.replace(F("#ICON#"), F("developer-board"));
            PublishMqtt(topic.c_str(), payload.c_str(), true);

            // Wifi SSID
            configPayloadTemplate = String(F(
                "{"
                "\"dev\":{"
                "\"ids\":\"#DEVICEID#\","
                "\"name\":\"#HOSTNAME#\","
                "\"mdl\":\"PixelIt\","
                "\"mf\":\"PixelIt\","
                "\"sw\":\"#VERSION#\","
                "\"cu\":\"#IP#\""
                "},"
                "\"avty_t\":\"#MASTERTOPIC#state\","
                "\"pl_avail\":\"connected\","
                "\"pl_not_avail\":\"disconnected\","
                "\"uniq_id\":\"#DEVICEID##SENSORID#\","
                "\"name\":\"#SENSORNAME#\","
                "\"stat_t\":\"#MASTERTOPIC##STATETOPIC#\","
                "\"val_tpl\":\"{{value_json.#VALUENAME#}}\","
                "\"ent_cat\":\"diagnostic\","
                "\"ic\":\"mdi:#ICON#\","
                "\"enabled_by_default\":\"false\""
                "}"));
            configPayloadTemplate.replace(" ", "");
            configPayloadTemplate.replace(F("#DEVICEID#"), deviceID);
            configPayloadTemplate.replace(F("#HOSTNAME#"), hostname);
            configPayloadTemplate.replace(F("#VERSION#"), VERSION);
            if (mqttUseDeviceTopic)
            {
                configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttDeviceTopic);
            }
            else
            {
                configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttMasterTopic);
            }
            configPayloadTemplate.replace(F("#IP#"), ip_url);

            topic = configTopicTemplate;
            topic.replace(F("#COMPONENT#"), F("sensor"));
            topic.replace(F("#SENSORID#"), F("WifiSSID"));

            payload = configPayloadTemplate;
            payload.replace(F("#SENSORID#"), F("WifiSSID"));
            payload.replace(F("#SENSORNAME#"), F("SSID"));
            payload.replace(F("#STATETOPIC#"), F("matrixinfo"));
            payload.replace(F("#VALUENAME#"), F("wifiSSID"));
            payload.replace(F("#ICON#"), F("wifi"));
            PublishMqtt(topic.c_str(), payload.c_str(), true);

            // Wifi BSSID
            topic = configTopicTemplate;
            topic.replace(F("#COMPONENT#"), F("sensor"));
            topic.replace(F("#SENSORID#"), F("WifiBSSID"));

            payload = configPayloadTemplate;
            payload.replace(F("#SENSORID#"), F("WifiBSSID"));
            payload.replace(F("#SENSORNAME#"), F("BSSID"));
            payload.replace(F("#STATETOPIC#"), F("matrixinfo"));
            payload.replace(F("#VALUENAME#"), F("wifiBSSID"));
            payload.replace(F("#ICON#"), F("wifi"));
            PublishMqtt(topic.c_str(), payload.c_str(), true);

            // Chip ID
            topic = configTopicTemplate;
            topic.replace(F("#COMPONENT#"), F("sensor"));
            topic.replace(F("#SENSORID#"), F("chipID"));

            payload = configPayloadTemplate;
            payload.replace(F("#SENSORID#"), F("chipID"));
            payload.replace(F("#SENSORNAME#"), F("Chip ID"));
            payload.replace(F("#STATETOPIC#"), F("matrixinfo"));
            payload.replace(F("#VALUENAME#"), F("chipID"));
            payload.replace(F("#ICON#"), F("developer-board"));
            PublishMqtt(topic.c_str(), payload.c_str(), true);

            // LED Matrix on/off + brightness light
            configPayloadTemplate = String(F(
                "{"
                "\"dev\":{"
                "\"ids\":\"#DEVICEID#\","
                "\"name\":\"#HOSTNAME#\","
                "\"mdl\":\"PixelIt\","
                "\"mf\":\"PixelIt\","
                "\"sw\":\"#VERSION#\","
                "\"cu\":\"#IP#\""
                "},"
                "\"avty_t\":\"#MASTERTOPIC#state\","
                "\"pl_avail\":\"connected\","
                "\"pl_not_avail\":\"disconnected\","
                "\"uniq_id\":\"#DEVICEID##SENSORID#\","
                "\"name\":\"#SENSORNAME#\","
                "\"schema\":\"template\","
                "\"stat_t\":\"#MASTERTOPIC##STATETOPIC#\","
                "\"stat_tpl\":\"{{ \'on\' if value_json.sleepMode is false else \'off\' }}\","
                "\"cmd_t\":\"#MASTERTOPIC##COMMANDTOPIC#\","
                "\"cmd_on_tpl\":\"{\\\"sleepMode\\\": false {%- if brightness is defined -%}, \\\"brightness\\\": {{ brightness }}{%- endif -%}}\","
                "\"cmd_off_tpl\":\"{\\\"sleepMode\\\": true}\","
                "\"bri_tpl\":\"{{ value_json.currentMatrixBrightness }}\","
                "\"icon\":\"mdi:#ICON#\""
                "}"));
            configPayloadTemplate.replace(F("#DEVICEID#"), deviceID);
            configPayloadTemplate.replace(F("#HOSTNAME#"), hostname);
            configPayloadTemplate.replace(F("#VERSION#"), VERSION);
            if (mqttUseDeviceTopic)
            {
                configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttDeviceTopic);
            }
            else
            {
                configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttMasterTopic);
            }
            configPayloadTemplate.replace(F("#IP#"), ip_url);

            topic = configTopicTemplate;
            topic.replace(F("#COMPONENT#"), F("light"));
            topic.replace(F("#SENSORID#"), F("LEDMatrixLight"));

            payload = configPayloadTemplate;
            payload.replace(F("#SENSORID#"), F("LEDMatrixLight"));
            payload.replace(F("#SENSORNAME#"), F("LED Matrix"));
            payload.replace(F("#STATETOPIC#"), F("matrixinfo"));
            payload.replace(F("#COMMANDTOPIC#"), F("setScreen"));
            payload.replace(F("#ICON#"), F("led-strip"));
            PublishMqtt(topic.c_str(), payload.c_str(), true);

            Log(F("MQTTreconnect"), F("MQTT discovery information published"));
        }
    }
    else
    {
        Log(F("MQTTreconnect"), F("MQTT connect failed! Retry in a few seconds..."));
    }

    return connected;
}
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
// Effekte


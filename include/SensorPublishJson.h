#pragma once
#include "SensorPublishPolicy.h"
#include <ArduinoJson.h>

inline bool WriteSensorPublishSettings(JsonObject &root, const SensorPublishSettings &s)
{
    JsonObject &json = root.createNestedObject("mqttSensorPublish");
    json["periodic"] = s.periodic;
    json["interval"] = s.interval;
    json["minInterval"] = s.minInterval;
    json["maxInterval"] = s.maxInterval;
    json["temperature"] = s.temperature;
    json["humidity"] = s.humidity;
    json["pressure"] = s.pressure;
    json["gas"] = s.gas;
    json["battery"] = s.battery;
    json["lux"] = s.lux;
    return json.success() && json.size() == 10;
}

// Apply partial objects atomically. Invalid types/ranges preserve all settings.
inline bool ReadSensorPublishSettings(JsonVariant value, SensorPublishSettings &settings)
{
    if (!value.is<JsonObject &>()) return false;
    JsonObject &json = value.as<JsonObject &>();
    SensorPublishSettings candidate = settings;
    if (json.containsKey("periodic"))
    {
        if (!json["periodic"].is<bool>()) return false;
        candidate.periodic = json["periodic"].as<bool>();
    }
    const char *times[] = {"interval", "minInterval", "maxInterval"};
    uint32_t *targets[] = {&candidate.interval, &candidate.minInterval, &candidate.maxInterval};
    for (unsigned i = 0; i < 3; ++i)
    {
        if (!json.containsKey(times[i])) continue;
        JsonVariant v = json[times[i]];
        if (!v.is<unsigned long>()) return false;
        const unsigned long n = v.as<unsigned long>();
        if (n > 86400 || (i == 0 && n < 3)) return false;
        *targets[i] = n;
    }
    const uint32_t minimum = candidate.periodic ? candidate.interval : candidate.minInterval;
    if (candidate.maxInterval && candidate.maxInterval < minimum) return false;
    const char *keys[] = {"temperature", "humidity", "pressure", "gas", "battery", "lux"};
    float *thresholds[] = {&candidate.temperature, &candidate.humidity, &candidate.pressure,
        &candidate.gas, &candidate.battery, &candidate.lux};
    for (unsigned i = 0; i < 6; ++i)
    {
        if (!json.containsKey(keys[i])) continue;
        JsonVariant v = json[keys[i]];
        if (!v.is<float>()) return false;
        const float n = v.as<float>();
        if (!isfinite(n) || n < 0 || n > 1000000) return false;
        *thresholds[i] = n;
    }
    settings = candidate;
    return true;
}

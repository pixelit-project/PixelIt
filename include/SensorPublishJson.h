#pragma once
#include "SensorPublishPolicy.h"
#include <ArduinoJson.h>
#include <cmath>

inline bool WriteSensorPublishSettings(JsonObject &root, const SensorPublishSettings &s)
{
    JsonObject &existing = root["mqttSensorPublish"].as<JsonObject &>();
    JsonObject &json = existing.success() ? existing : root.createNestedObject("mqttSensorPublish");
    return json.success() &&
        json.set("periodic", s.periodic) &&
        json.set("dynamic", s.dynamic) &&
        json.set("interval", s.interval) &&
        json.set("minInterval", s.minInterval) &&
        json.set("maxInterval", s.maxInterval) &&
        json.set("temperature", s.temperature) &&
        json.set("humidity", s.humidity) &&
        json.set("pressure", s.pressure) &&
        json.set("gas", s.gas) &&
        json.set("battery", s.battery) &&
        json.set("lux", s.lux);
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
    if (json.containsKey("dynamic"))
    {
        if (!json["dynamic"].is<bool>()) return false;
        candidate.dynamic = json["dynamic"].as<bool>();
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
    if (candidate.dynamic && candidate.maxInterval && candidate.maxInterval < minimum) return false;
    const char *keys[] = {"temperature", "humidity", "pressure", "gas", "battery", "lux"};
    float *thresholds[] = {&candidate.temperature, &candidate.humidity, &candidate.pressure,
        &candidate.gas, &candidate.battery, &candidate.lux};
    for (unsigned i = 0; i < 6; ++i)
    {
        if (!json.containsKey(keys[i])) continue;
        JsonVariant v = json[keys[i]];
        if (!v.is<float>()) return false;
        const float n = v.as<float>();
        if (!std::isfinite(n) || n < 0 || n > 1000000) return false;
        *thresholds[i] = n;
    }
    settings = candidate;
    return true;
}

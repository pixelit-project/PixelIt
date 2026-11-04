#pragma once
#include <stdint.h>
#include <math.h>
#include <float.h>

// Fixed storage only: no JSON or pixel buffers are retained by the policy.
struct SensorPublishSettings
{
    bool periodic = false;
    uint32_t interval = 60, minInterval = 0, maxInterval = 0; // seconds
    float temperature = 0, humidity = 0, pressure = 0, gas = 0, battery = 0, lux = 0;
};

struct SensorSample
{
    float values[5] = {};
    // 0 = unavailable, 1 = finite number, 2 = read error
    uint8_t states[5] = {};
};

struct SensorPublishState
{
    SensorSample sent;
    uint32_t lastSent = 0, lastChecked = 0;
    bool initialized = false;

    bool due(const SensorSample &sample, const float *thresholds,
             const SensorPublishSettings &settings, uint32_t now, bool force = false)
    {
        if (force || !initialized) return true;
        const uint32_t elapsed = now - lastSent;
        if (settings.maxInterval && elapsed >= settings.maxInterval * 1000UL) return true;
        if (settings.periodic)
        {
            if (uint32_t(now - lastChecked) < settings.interval * 1000UL) return false;
        }
        else if (elapsed < settings.minInterval * 1000UL) return false;
        for (unsigned i = 0; i < 5; ++i)
        {
            if (sample.states[i] != sent.states[i]) return true;
            if (sample.states[i] != 1) continue;
            const float delta = fabsf(sample.values[i] - sent.values[i]);
            // Zero means any actual change, never unchanged readings.
            const float tolerance = FLT_EPSILON *
                fmaxf(fabsf(sample.values[i]), fabsf(sent.values[i]));
            if (delta > 0 && delta + tolerance >= thresholds[i]) return true;
        }
        if (settings.periodic) lastChecked = now;
        return false;
    }

    void commit(const SensorSample &sample, uint32_t now)
    {
        sent = sample;
        lastSent = lastChecked = now;
        initialized = true;
    }
};

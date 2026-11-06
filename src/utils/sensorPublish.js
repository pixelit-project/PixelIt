// Matches the firmware's bounded, atomic mqttSensorPublish validation.
export function sensorPublishError(settings) {
    if (!settings || typeof settings.periodic !== 'boolean') return 'Choose a publishing mode.';
    if (settings.dynamic !== undefined && typeof settings.dynamic !== 'boolean') return 'Choose whether to send on change.';
    for (const key of ['interval', 'minInterval', 'maxInterval']) {
        const value = settings[key];
        if (!Number.isInteger(value) || value < (key === 'interval' ? 3 : 0) || value > 86400) {
            return key === 'interval' ? 'Check interval must be 3–86400 whole seconds.' : 'Send intervals must be 0–86400 whole seconds.';
        }
    }
    const minimum = settings.periodic ? settings.interval : settings.minInterval;
    if (settings.dynamic !== false && settings.maxInterval && settings.maxInterval < minimum) return 'The periodic send interval must be at least the active check or minimum send interval.';
    for (const key of ['temperature', 'humidity', 'pressure', 'gas', 'battery', 'lux']) {
        const value = settings[key];
        if (typeof value !== 'number' || !Number.isFinite(value) || value < 0 || value > 1000000) return 'Thresholds must be numbers from 0 to 1000000.';
    }
    return '';
}

export function matrixInfoIntervalError(value) {
    if (value !== undefined && (!Number.isInteger(value) || value < 10 || value > 86400)) {
        return 'Matrix info interval must be 10–86400 whole seconds.';
    }
    return '';
}

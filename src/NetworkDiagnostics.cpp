#include "FirmwareState.h"

#if defined(ESP32)
// WiFi callbacks run on the event task. Never log, render or touch clients here.
void InitNetworkDiagnostics()
{
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
        const uint32_t timestamp = millis();
        portENTER_CRITICAL(&networkDiagnosticsMux);
        if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED)
        {
            ++networkDiagnostics.wifiDisconnects;
            networkDiagnostics.wifiLastDisconnectMillis = timestamp;
            networkDiagnostics.wifiLastReason = info.wifi_sta_disconnected.reason;
        }
        else if (event == ARDUINO_EVENT_WIFI_STA_GOT_IP)
        {
            ++networkDiagnostics.wifiGotIP;
            networkDiagnostics.wifiLastGotIPMillis = timestamp;
        }
        portEXIT_CRITICAL(&networkDiagnosticsMux);
    });
}

void RecordMqttDuration(bool reconnect, unsigned long started)
{
    const uint32_t timestamp = millis();
    const uint32_t elapsed = timestamp - started;
    const int state = client.state();
    portENTER_CRITICAL(&networkDiagnosticsMux);
    uint32_t &maximum = reconnect ? networkDiagnostics.mqttMaxConnectMillis : networkDiagnostics.mqttMaxLoopMillis;
    if (elapsed > maximum) maximum = elapsed;
    if (elapsed >= 250)
    {
        if (reconnect) ++networkDiagnostics.mqttSlowConnects;
        else ++networkDiagnostics.mqttSlowLoops;
        networkDiagnostics.mqttLastSlowMillis = timestamp;
        networkDiagnostics.mqttLastSlowDuration = elapsed;
        networkDiagnostics.mqttLastSlowState = state;
        networkDiagnostics.mqttLastSlowWasConnect = reconnect;
    }
    portEXIT_CRITICAL(&networkDiagnosticsMux);
}

void AddNetworkDiagnostics(JsonObject &root)
{
    portENTER_CRITICAL(&networkDiagnosticsMux);
    const NetworkDiagnostics snapshot = networkDiagnostics;
    portEXIT_CRITICAL(&networkDiagnosticsMux);
    // Allocate only on explicit diagnostic requests, never in the event task.
    JsonObject &network = root.createNestedObject("network");
    if (!network.success()) return;
    network["wifiStatus"] = static_cast<int>(WiFi.status());
    network["wifiDisconnects"] = snapshot.wifiDisconnects;
    network["wifiGotIP"] = snapshot.wifiGotIP;
    network["wifiLastReason"] = snapshot.wifiLastReason;
    network["wifiLastDisconnectMillis"] = snapshot.wifiLastDisconnectMillis;
    network["wifiLastGotIPMillis"] = snapshot.wifiLastGotIPMillis;
    network["mqttSlowLoops"] = snapshot.mqttSlowLoops;
    network["mqttSlowConnects"] = snapshot.mqttSlowConnects;
    network["mqttMaxLoopMillis"] = snapshot.mqttMaxLoopMillis;
    network["mqttMaxConnectMillis"] = snapshot.mqttMaxConnectMillis;
    network["mqttLastSlowMillis"] = snapshot.mqttLastSlowMillis;
    network["mqttLastSlowDuration"] = snapshot.mqttLastSlowDuration;
    network["mqttLastSlowState"] = snapshot.mqttLastSlowState;
    network["mqttLastSlowWasConnect"] = snapshot.mqttLastSlowWasConnect;
}
#endif

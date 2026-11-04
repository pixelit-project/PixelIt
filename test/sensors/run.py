#!/usr/bin/env python3
"""Host tests for sensor publishing policy, settings and actual notifications."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
lib = root / '.pio/libdeps/ESP8266_d1_mini/ArduinoJson/src'
source = (root / 'src/Notifications.cpp').read_text()
ldr = source[source.index('void SendLDR('):source.index('bool sendLiveview(')]
sensor = source[source.index('void SendSensor('):source.index('void SendConfig(')]
sampling = (root / 'src/Sensors.cpp').read_text()
sampling = sampling[sampling.index('    if (sample)'):sampling.index('    root["hostname"] = hostname;')]

program = r'''
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <limits>
#include "SensorPublishJson.h"
using String = std::string;
using uint = unsigned;
uint32_t tick = 0;
uint32_t millis() { return tick; }
bool mqttAktiv = true, mqttUseDeviceTopic = false;
String mqttMasterTopic = "pixelit/", mqttDeviceTopic = "pixelit/device/";
String oldGetSensor, oldGetLuxSensor;
SensorPublishSettings mqttSensorPublish;
SensorPublishState mqttSensorState, mqttLuxState;
float currentLux = 100;
SensorSample reading;
String payload = "initial";
struct { bool online = true; bool connected() { return online; } } client;
struct { unsigned connectedClients() { return 1; } } webSocket;
bool websocketConnection[1] = {true};
std::vector<String> mqtt, ws;
bool failPublish = false;
bool PublishMqtt(const char *topic, const char *, bool) {
 mqtt.push_back(topic); return !failPublish;
}
void SendWebSocketJson(unsigned, const String &text) { ws.push_back(text); }
String GetSensor(SensorSample *sample) { *sample = reading; return payload; }
String GetLuxSensor() { return std::to_string(currentLux); }
''' + 'void capture(JsonObject &root, SensorSample *sample) {\n' + sampling + '}\n' + ldr + sensor + r'''
int main() {
 // Capture the actual existing JSON values without reparsing/copying the payload.
 StaticJsonBuffer<512> captureBuffer;
 JsonObject &values = captureBuffer.createObject();
 values["temperature"] = 20.25f; values["humidity"] = 45;
 values["pressure"] = "Not installed"; values["gas"] = "Error while reading";
 values["battery"] = std::numeric_limits<float>::quiet_NaN();
 SensorSample captured; capture(values, &captured);
 assert(captured.states[0] == 1 && captured.values[0] == 20.25f);
 assert(captured.states[1] == 1 && captured.values[1] == 45);
 assert(captured.states[2] == 0 && captured.states[3] == 2 && captured.states[4] == 2);
 SensorPublishSettings cfg;
 SensorPublishState state;
 SensorSample a; a.states[0] = 1; a.values[0] = 20;
 float thresholds[5] = {0.5f, 0, 0, 0, 0};
 assert(state.due(a, thresholds, cfg, 0)); state.commit(a, 0);
 assert(!state.due(a, thresholds, cfg, 3000));
 a.values[0] = 20.25f; assert(!state.due(a, thresholds, cfg, 6000));
 a.values[0] = 20.5f; assert(state.due(a, thresholds, cfg, 9000));
 cfg.minInterval = 12;
 assert(!state.due(a, thresholds, cfg, 9000));
 assert(state.due(a, thresholds, cfg, 12000)); state.commit(a, 12000);
 a.values[0] = 20; assert(state.due(a, thresholds, cfg, 24000));
 cfg.periodic = true; cfg.interval = 60;
 assert(!state.due(a, thresholds, cfg, 27000));
 assert(state.due(a, thresholds, cfg, 72000));
 // Failed sends do not move baseline or postpone retry by another interval.
 assert(state.due(a, thresholds, cfg, 75000)); state.commit(a, 75000);
 assert(!state.due(a, thresholds, cfg, 135000));
 a.values[0] = 21;
 assert(!state.due(a, thresholds, cfg, 138000));
 assert(state.due(a, thresholds, cfg, 195000));
 cfg.maxInterval = 120; a = state.sent;
 assert(state.due(a, thresholds, cfg, 195000));
 assert(state.due(a, thresholds, cfg, 75001, true));
 // Wrap-safe millis arithmetic and error/recovery transitions.
 cfg = SensorPublishSettings(); state.commit(a, UINT32_MAX - 999);
 cfg.minInterval = 3; a.states[0] = 2;
 assert(!state.due(a, thresholds, cfg, 1999));
 assert(state.due(a, thresholds, cfg, 2000)); state.commit(a, 2000);
 a.states[0] = 1; assert(state.due(a, thresholds, cfg, 5000));
 thresholds[0] = 0; state.commit(a, 5000);
 assert(!state.due(a, thresholds, cfg, 8000));
 a.values[0] += 0.01f; assert(state.due(a, thresholds, cfg, 8000));
 // Decimal thresholds remain inclusive despite float representation.
 cfg.minInterval = 0; a.values[0] = 20.1f; state.commit(a, 0);
 thresholds[0] = 0.2f; a.values[0] = 20.3f;
 assert(state.due(a, thresholds, cfg, 3000));
 a.values[0] = 20.29f; assert(!state.due(a, thresholds, cfg, 3000));
 // Settings round trip, partial updates, strict validation and atomic rejection.
 StaticJsonBuffer<2048> buffer;
 JsonObject &json = buffer.createObject(); assert(WriteSensorPublishSettings(json, cfg));
 StaticJsonBuffer<32> scarce;
 assert(!WriteSensorPublishSettings(scarce.createObject(), cfg));
 SensorPublishSettings parsed;
 assert(ReadSensorPublishSettings(json["mqttSensorPublish"], parsed));
 assert(parsed.minInterval == 0 && parsed.temperature == 0);
 const char *valid = "{\"periodic\":true,\"interval\":60,\"maxInterval\":900,\"temperature\":0.2,\"pressure\":0.5}";
 assert(ReadSensorPublishSettings(buffer.parseObject(valid), parsed));
 assert(parsed.periodic && parsed.maxInterval == 900 && parsed.pressure == 0.5f);
 const char *bad[] = {"null", "[]", "{\"periodic\":1}", "{\"interval\":2}",
  "{\"interval\":3.5}", "{\"interval\":\"60\"}", "{\"minInterval\":-1}",
  "{\"maxInterval\":86401}", "{\"maxInterval\":30}", "{\"temperature\":-1}",
  "{\"temperature\":true}", "{\"temperature\":\"0.2\"}", "{\"lux\":1000001}"};
 for (auto text : bad) {
  StaticJsonBuffer<512> b; JsonVariant v = b.parse(text);
  assert(!ReadSensorPublishSettings(v, parsed));
  assert(parsed.periodic && parsed.maxInterval == 900 && parsed.temperature == 0.2f);
 }
 // Actual notifications: shared payload baseline, WebSocket independence,
 // failed publish retry, lux independence and optional device + legacy topics.
 reading.states[0] = 1; reading.values[0] = 20;
 mqttSensorPublish.temperature = 0.5f;
 tick = 3000; SendSensor(false); assert(mqtt.size() == 2 && ws.size() == 1);
 mqtt.clear(); ws.clear(); reading.values[0] = 20.25f; payload = "small change";
 tick = 6000; SendSensor(false); assert(mqtt.empty() && ws.size() == 1);
 reading.values[0] = 20.5f; payload = "threshold"; failPublish = true;
 tick = 9000; SendSensor(false); assert(mqtt.size() == 2);
 assert(mqttSensorState.sent.values[0] == 20);
 failPublish = false; mqtt.clear(); ws.clear(); mqttUseDeviceTopic = true;
 tick = 12000; SendSensor(false); assert(mqtt.size() == 4 && ws.empty());
 assert(mqttSensorState.sent.values[0] == 20.5f);
 mqtt.clear(); SendSensor(false); assert(mqtt.empty());
 client.online = false; reading.values[0] = 21; payload = "offline";
 SendSensor(false); assert(mqtt.empty() && ws.size() == 1);
 client.online = true; SendSensor(false); assert(mqtt.size() == 4);
 mqtt.clear(); mqttSensorPublish.lux = 10;
 SendLDR(false); assert(mqtt.size() == 2); mqtt.clear();
 currentLux = 105; SendLDR(false); assert(mqtt.empty());
 currentLux = 110; SendLDR(false); assert(mqtt.size() == 2);
 mqtt.clear(); SendSensor(true); assert(mqtt.size() == 4);
}
'''
with tempfile.TemporaryDirectory() as tmp:
    cpp = Path(tmp) / 'test.cpp'
    cpp.write_text(program)
    exe = Path(tmp) / 'test'
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-Werror',
                    '-I'+str(root/'include'), '-isystem', str(lib), str(cpp), '-o', str(exe)], check=True)
    subprocess.run([str(exe)], check=True)
print('Sensor publishing tests passed')

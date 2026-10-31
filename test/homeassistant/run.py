#!/usr/bin/env python3
"""Host regression tests for the firmware's actual button state machine."""
from pathlib import Path
import json
import re
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'src/Buttons.cpp').read_text()
service = source[source.index('void ServiceButtons()'):].replace("unsigned long", "uint32_t")
program = r'''
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
using uint = unsigned int;
uint32_t tick = 0;
bool btnEnabled[3] = {true, true, true};
int btnPin[3] = {0, 1, 2}, btnPressedLevel[3] = {1, 1, 1};
bool inputs[3] = {}, states[3] = {};
std::vector<std::string> events[3];
uint32_t millis() { return tick; }
int TranslatePin(int pin) { return pin; }
int digitalRead(int pin) { return inputs[pin]; }
void PublishButtonEvent(uint b, const char *event) { events[b].push_back(event); }
void HandleAndSendButtonPress(uint b, bool state) {
 states[b]=state; PublishButtonEvent(b, state ? "press" : "release");
}
''' + service + r'''
void advance(unsigned long ms) { while (ms--) { ++tick; ServiceButtons(); } }
void input(int b, bool value, unsigned long ms=31) { inputs[b]=value; advance(ms); }
void expect(int b, std::vector<std::string> wanted) { assert(events[b]==wanted); events[b].clear(); }
int main() {
 // Contact bounce must not produce actions or events.
 input(0,true,10); input(0,false,10); input(0,true,10); input(0,false,40);
 expect(0,{});
 input(0,true); assert(states[0]); input(0,false); advance(351);
 expect(0,{"press","release","single_press"});
 input(0,true); input(0,false); advance(100); input(0,true); input(0,false); advance(400);
 expect(0,{"press","release","press","release","double_press"});
 input(0,true); advance(900); input(0,false); advance(400);
 expect(0,{"press","long_press","release"});
 // Independent buttons and disabled hardware.
 input(0,true); input(1,true); input(0,false); input(1,false); advance(400);
 expect(0,{"press","release","single_press"});
 expect(1,{"press","release","single_press"});
 btnEnabled[2]=false; input(2,true,1000); input(2,false); expect(2,{});
 // Unsigned subtraction keeps debounce and hold detection working over rollover.
 tick=0xfffffff0UL; input(0,true); advance(801); input(0,false); advance(400);
 expect(0,{"press","long_press","release"});
}
'''
with tempfile.TemporaryDirectory() as temp:
    path = Path(temp)
    (path / 'buttons.cpp').write_text(program)
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', str(path / 'buttons.cpp'), '-o', str(path / 'buttons')], check=True)
    subprocess.run([str(path / 'buttons')], check=True)
print('Button regression tests passed')

# Exercise the actual discovery builder/scheduler using ArduinoJson on the host.
discovery = (root / 'src/Mqtt.cpp').read_text()
discovery = discovery[discovery.index('namespace\n{'):discovery.index('\nvoid QueueMqttConfig')]
discovery = discovery.replace('String(button)', 'std::to_string(button)').replace('String(state - 3)', 'std::to_string(state - 3)')
discovery = re.sub(r'\bString\b', 'std::string', discovery)
json_include = next((root / '.pio/libdeps').glob('*/ArduinoJson/src'))
program = r'''
#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>
#include <ArduinoJson.h>
#define F(x) x
#define VERSION "test"
using uint = unsigned int;
std::string mqttDeviceTopic="device/", mqttMasterTopic="master/", deviceID="PixelIt-test", hostname="Test device";
bool mqttUseDeviceTopic=false, mqttHAdiscoverable=true, animateBMPAktivLoop=false;
bool btnEnabled[3]={true,true,true}, btnLastPublishState[3]={false,true,false};
const char *btnLogNames[3]={"Left","Middle","Right"};
enum {TempSensor_None,TempSensor_BMP280,TempSensor_BME280,TempSensor_BME680};
enum {TemperatureUnit_Celsius,TemperatureUnit_Fahrenheit};
int tempSensor=TempSensor_BME680, temperatureUnit=TemperatureUnit_Celsius;
unsigned long tick=1000, animateBMPPrevMillis=0, animateBMPDelay=150;
uint32_t animationRenderedFrames=0;
unsigned long millis() { return tick; }
struct IP { std::string toString() { return "192.0.2.1"; } };
struct Wifi { IP localIP() { return IP(); } } WiFi;
struct Client { bool connected() { return true; } } client;
struct Message {std::string topic,payload;};
std::vector<Message> messages;
bool PublishMqtt(const char *topic,const char *payload,bool retained=false) {
 assert(retained); messages.push_back({topic,payload}); return true;
}
std::string GetMatrixInfo() { return "{}"; }
std::string GetSensor() { return "{}"; }
std::string GetLuxSensor() { return "{}"; }
''' + discovery + r'''
void run() {
 for(int i=0;i<100 && haDiscoveryPending;i++) {
  tick+=150;
  size_t old=messages.size();
  ServiceHADiscovery();
  assert(messages.size()-old<=1);
 }
 assert(!haDiscoveryPending);
}
int main() {
 RequestHADiscovery(); run();
 assert(messages.size()==38);
 for (const auto &m:messages) std::cout<<m.topic<<"\t"<<m.payload<<"\n";
 // Clean up every entity when discovery is disabled, retaining initial states.
 mqttHAdiscoverable=false; messages.clear(); RequestHADiscovery(); run();
 assert(messages.size()==38);
 for(int i=0;i<32;i++) assert(messages[i].payload.empty());
 mqttHAdiscoverable=true; mqttUseDeviceTopic=true; messages.clear();
 tempSensor=TempSensor_BMP280; btnEnabled[1]=false;
 RequestHADiscovery(); run();
 assert(messages[1].payload.empty()); // no humidity on BMP280
 assert(messages[3].payload.empty()); // no gas on BMP280
 assert(messages[28].payload.empty() && messages[29].payload.empty());
 DynamicJsonBuffer buffer;
 JsonObject &config=buffer.parseObject(messages[14].payload);
 assert(config["availability_topic"]=="device/state");
 // The scheduler must defer near the next LED deadline and advance after it.
 RequestHADiscovery(); animateBMPAktivLoop=true; animationRenderedFrames=1;
 tick+=1000; animateBMPPrevMillis=tick-140;
 assert(!HADiscoveryDue());
 size_t old=messages.size(); ServiceHADiscovery(); assert(messages.size()==old);
 animateBMPPrevMillis=tick; assert(HADiscoveryDue());
 ServiceHADiscovery(); assert(messages.size()==old+1);
 assert(!HADiscoveryDue());
}
'''
with tempfile.TemporaryDirectory() as temp:
    path = Path(temp)
    (path / 'discovery.cpp').write_text(program)
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-isystem', str(json_include), str(path / 'discovery.cpp'), '-o', str(path / 'discovery')], check=True)
    output = subprocess.check_output([str(path / 'discovery')], text=True)
configs = {}
for line in output.splitlines():
    topic, payload = line.split('\t', 1)
    if topic.endswith('/config'):
        config = json.loads(payload)
        assert '#' not in config['unique_id']
        assert config['availability_topic'] == 'master/state'
        configs[config['unique_id'].removeprefix('PixelIt-test')] = config
assert len(configs) == 32
assert configs['Humidity']['unique_id'] == 'PixelIt-testHumidity'
assert 'device_class' not in configs['VOC']
for identifier in ('AutoBrightness', 'ClockAutoFallback'):
    config = configs[identifier]
    for key in ('payload_on', 'payload_off'):
        command = json.loads(config[key])
        assert len(command) == 1 and isinstance(next(iter(command.values())), bool)
for identifier in ('Uptime', 'FreeHeap', 'ResetReason'):
    assert configs[identifier]['enabled_by_default'] is False
assert configs['Button0Event']['event_types'] == ['press','release','single_press','double_press','long_press']
print('Discovery payload, cleanup and scheduling tests passed')

assert configs['DisplayText']['command_topic'] == 'master/setTextOptions/text'
assert configs['ShowText']['command_topic'] == 'master/showText'
assert 'state_topic' not in configs['ShowText']
assert configs['TextFont']['options'] == ['Small', 'Large']
assert configs['TextAlignment']['options'] == ['Centered', 'Left']
assert configs['TextScroll']['options'] == ['Auto', 'Always', 'Off']
assert configs['TextColor']['pattern'] == '^#[0-9A-Fa-f]{6}$'
assert configs['TextColorPicker']['schema'] == 'template'
assert configs['TextColorPicker']['command_topic'] == 'master/setTextOptions/colorPicker'
assert all(key in configs['TextColorPicker'] for key in ('red_template', 'green_template', 'blue_template'))
assert 'brightness_template' not in configs['TextColorPicker']
assert configs['FirmwareUpdate']['device_class'] == 'firmware'
assert configs['FirmwareUpdate']['state_topic'] == 'master/matrixinfo'
assert 'latest_version_topic' not in configs['FirmwareUpdate']
assert configs['FirmwareUpdate']['value_template'] == "{{ {'installed_version': value_json.pixelitVersion, 'latest_version': value_json.latestReleaseVersion} | tojson }}"
assert 'command_topic' not in configs['FirmwareUpdate']
assert 'payload_install' not in configs['FirmwareUpdate']


# Exercise staged options and the actual screen JSON without display hardware.
text_source = (root / 'src/HAText.cpp').read_text()
text_source = '\n'.join(line for line in text_source.splitlines() if not line.startswith('#include'))
text_source = re.sub(r'\bString\b', 'std::string', text_source)
text_source = text_source.replace('if (!pendingHATextPayload.reserve(length)) return true;', 'pendingHATextPayload.reserve(length);')
text_source = text_source.replace('= static_cast<const char *>(nullptr)', '= \"\"')
program = r'''
#include <cassert>
#include <cstring>
#include <cstdint>
#include <string>
#include <vector>
#include <ArduinoJson.h>
using uint=unsigned int;
using byte=uint8_t;
#define MATRIX_HEIGHT 8
std::string mqttMasterTopic="master/", mqttDeviceTopic="device/";
bool mqttUseDeviceTopic=false;
std::string haTextMessage,haTextColor="#FFFFFF",haTextFont="Small",haTextAlignment="Centered",haTextScroll="Auto";
int PixelItFont=1;
struct Matrix {
 bool large=false;
 void setFont() { large=true; }
 void setFont(int*) { large=false; }
 void getTextBounds(const std::string &text,int,int,int16_t *x,int16_t *y,uint16_t *w,uint16_t *h) {
  *x=0; *y=large?0:-5; *w=text.size()*4; *h=text.empty()?0:large?8:5;
 }
} display;
Matrix *matrix=&display;
std::string Utf8ToAscii(const std::string &text) { return text; }
int updates=0;
void SendMatrixInfo() { ++updates; }
std::vector<std::string> screens;
void CreateFrames(JsonObject &screen) { std::string json; screen.printTo(json); screens.push_back(json); }
''' + text_source + r'''
bool send(const char *topic,const std::string &body) {
 return HandleHATextCommand(topic,reinterpret_cast<const byte*>(body.data()),body.size());
}
int main() {
 assert(!send("unrelated/topic","{}"));
 send("master/setTextOptions/text", "Hallo \"HA\" äöü");
 assert(haTextMessage=="Hallo \"HA\" äöü" && screens.empty());
 updates=0;
 send("master/setTextOptions/colorPicker", "#1020A0");
 assert(haTextColor=="#1020A0" && screens.empty());
 send("master/setTextOptions/colorPicker", "OFF"); assert(haTextColor=="#000000");
 send("master/setTextOptions/colorPicker", "ON"); assert(haTextColor=="#1020A0");
 send("master/setTextOptions/colorPicker", "invalid"); assert(haTextColor=="#1020A0");
 send("master/setTextOptions/color", "#ABCDEF");
 send("master/setTextOptions/colorPicker", "OFF");
 send("master/setTextOptions/colorPicker", "ON"); assert(haTextColor=="#ABCDEF");
 assert(screens.empty()); updates=0;
 assert(send("master/setTextOptions",R"({"text":"Hi \\\" HA","color":"#12aBcD","font":"Small","alignment":"Centered","scroll":"Auto"})"));
 assert(screens.empty()); assert(updates==1); assert(haTextColor=="#12aBcD");
 const auto old=haTextMessage;
 send("master/setTextOptions",R"({"text":"should not apply","color":"red"})");
 assert(haTextMessage==old && updates==1);
 send("master/setTextOptions",R"({"font":"Huge"})"); assert(haTextFont=="Small");
 send("master/showText","invalid"); assert(screens.empty());
 send("master/showText","PRESS"); assert(screens.size()==2);
 DynamicJsonBuffer buffer;
 JsonObject &wake=buffer.parseObject(screens[0]); assert(wake["sleepMode"]==false);
 JsonObject &screen=buffer.parseObject(screens[1]);
 assert(!screen.containsKey("sleepMode"));
 assert(screen["text"]["hexColor"]=="#12aBcD");
 assert(screen["text"]["centerText"]==true);
 assert(screen["text"]["position"]["y"]==1);
 assert(screen["text"]["scrollText"]=="auto");
 screens.clear();
 send("master/setTextOptions",R"({"font":"Large","alignment":"Left","scroll":"Off"})");
 assert(screens.empty());
 send("master/showText","PRESS");
 JsonObject &large=buffer.parseObject(screens[1]);
 assert(large["text"]["bigFont"]==1);
 assert(large["text"]["centerText"]==false);
 assert(large["text"]["position"]["y"]==1);
 assert(large["text"]["scrollText"]==false);
 screens.clear(); send("master/setTextOptions",R"({"scroll":"Always"})");
 send("master/setText","Legacy");
 assert(haTextMessage=="Legacy" && screens.size()==2);
 JsonObject &always=buffer.parseObject(screens[1]); assert(always["text"]["scrollText"]==true);
 screens.clear(); send("master/setText",std::string(256,'x')); assert(screens.empty());
 assert(!send("device/setTextOptions",R"({"text":"device"})"));
 mqttUseDeviceTopic=true; send("device/setTextOptions",R"({"text":"device"})");
 assert(haTextMessage=="device" && screens.empty());
 send("device/setTextOptions",R"({"text":""})"); send("device/showText","PRESS");
 assert(haTextMessage.empty() && screens.size()==2);
 screens.clear();
 const std::string queued="queued text";
 assert(QueueHATextCommand("master/setTextOptions/text",reinterpret_cast<const byte*>(queued.data()),queued.size()));
 assert(haTextMessage.empty() && screens.empty());
 ServiceHAText(); assert(haTextMessage==queued && screens.empty());
 ServiceHAText(); assert(screens.empty());
 assert(QueueHATextCommand("master/showText",reinterpret_cast<const byte*>("PRESS"),5));
 assert(screens.empty()); ServiceHAText(); assert(screens.size()==2);
}
'''
with tempfile.TemporaryDirectory() as temp:
    path = Path(temp)
    (path / 'text.cpp').write_text(program)
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-isystem', str(json_include), str(path / 'text.cpp'), '-o', str(path / 'text')], check=True)
    subprocess.run([str(path / 'text')], check=True)
print('Text staging, validation and rendering-command tests passed')

runtime = (root / 'src/Runtime.cpp').read_text()
update_check = runtime[runtime.index('void checkUpdate()'):runtime.index('\nvoid RecordAnimationWork')]
update_check = update_check.replace('String(statusCode)', 'std::to_string(statusCode)')
update_check = re.sub(r'\bString\b', 'std::string', update_check)
program = r'''
#include <cassert>
#include <string>
#include <ArduinoJson.h>
#define F(x) x
#define VERSION "2.7.0"
#define CHECKUPDATE_SERVER_HOST "example.invalid"
#define CHECKUPDATE_SERVER_PORT 80
#define CHECKUPDATE_SERVER_PATH "/api/lastversion"
int wifiClientHTTP=0, httpStatus=200, notifications=0, closed=0;
std::string response=R"({"version":"2.8.0"})", lastReleaseVersion=VERSION;
bool firmwareUpdateChecked=false;
struct HttpClient {
 HttpClient(int,const char*,int) {}
 void sendHeader(const char*,const char*) {}
 void setTimeout(int) {}
 void setHttpResponseTimeout(int) {}
 void get(const char*) {}
 int responseStatusCode() { return httpStatus; }
 std::string responseBody() { return response; }
 void stop() { ++closed; }
};
void Log(const char*,const std::string&) {}
void SendMatrixInfo() { ++notifications; }
int compareVersions(const char *a,const char *b) { return std::string(a).compare(b); }
''' + update_check + r'''
int main() {
 checkUpdate(); assert(firmwareUpdateChecked && lastReleaseVersion=="2.8.0" && notifications==1);
 firmwareUpdateChecked=false; httpStatus=503; checkUpdate();
 assert(!firmwareUpdateChecked && notifications==1 && lastReleaseVersion=="2.8.0");
 httpStatus=200;
 const char *invalid[]={"{}","not json",R"({"version":42})",R"({"version":""})"};
 for(const char *value:invalid) { response=value; checkUpdate(); }
 assert(!firmwareUpdateChecked && notifications==1 && lastReleaseVersion=="2.8.0");
 response=R"({"version":"2.7.0"})"; checkUpdate();
 assert(firmwareUpdateChecked && lastReleaseVersion=="2.7.0" && notifications==2);
 assert(closed==7);
}
'''
with tempfile.TemporaryDirectory() as temp:
    path = Path(temp)
    (path / 'update.cpp').write_text(program)
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', '-isystem', str(json_include), str(path / 'update.cpp'), '-o', str(path / 'update')], check=True)
    subprocess.run([str(path / 'update')], check=True)
print('Firmware version polling and failed-response tests passed')

mqtt_source = (root / 'src/Mqtt.cpp').read_text()
buffer_service = mqtt_source[mqtt_source.index('bool PrepareMqttServiceBuffer()'):]
program = r'''
#include <cassert>
#include <cstdint>
const uint16_t MQTT_BUFFER_SIZE=11264;
struct Client {
 uint16_t memoryLimit=11264, size=256;
 bool setBufferSize(uint16_t requested) {
  if(requested>memoryLimit) return false;
  size=requested; return true;
 }
} client;
''' + buffer_service + r'''
int main() {
 assert(PrepareMqttServiceBuffer() && client.size==11264);
 client.memoryLimit=2048; client.size=256;
 assert(PrepareMqttServiceBuffer() && client.size==2048);
 client.memoryLimit=1024; client.size=256;
 assert(!PrepareMqttServiceBuffer() && client.size==256);
 client.memoryLimit=11264;
 assert(PrepareMqttServiceBuffer() && client.size==11264);
}
'''
with tempfile.TemporaryDirectory() as temp:
    path = Path(temp)
    (path / 'buffer.cpp').write_text(program)
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', str(path / 'buffer.cpp'), '-o', str(path / 'buffer')], check=True)
    subprocess.run([str(path / 'buffer')], check=True)
print('MQTT control-buffer fallback and recovery tests passed')

"""Exercise deferred status snapshots and the actual ESP8266 MQTT writer."""
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[2]
state = (root / 'include/FirmwareState.h').read_text()
responses = state[state.index('enum MqttResponse :'):state.index('extern uint8_t mqttPendingResponses;')]
timers = state[state.index('struct MqttWorkStats'):state.index('extern const int MQTT_RECONNECT_INTERVAL;')]
timers = timers.replace('extern MqttWorkStats mqttWorkStats;', 'MqttWorkStats mqttWorkStats;')
notifications = (root / 'src/Notifications.cpp').read_text()
notifications = notifications[notifications.index('void SendMatrixInfo()'):notifications.index('void SendLDR(')]
notifications = notifications.replace('String', 'std::string').replace('uint ', 'unsigned ')
program = r'''
#include <cassert>
#include <cstdint>
#include <string>
#include <vector>
bool mqttReceiveActive=false, mqttAktiv=true, deviceTopics=true;
uint8_t mqttPendingResponses=0;
unsigned long lastMqttMatrixInfoMillis=0;
std::string mqttMasterTopic="master/", mqttDeviceTopic="device/";
struct Client { bool online=true; unsigned buffer=256; bool connected(){return online;} } client;
struct Sockets { unsigned viewers=1; unsigned connectedClients(){return viewers;} } webSocket;
std::string websocketConnection[2];
bool MqttDeviceTopicsEnabled(){return deviceTopics;}
unsigned rendered=0, snapshots=0, socketMessages=0;
uint32_t now=123;
uint32_t millis(){return now;}
void ServiceDisplayFrames(){++rendered;}
std::string snapshot(const char *name){
 assert(!mqttReceiveActive && client.buffer==256); ++snapshots; return name;
}
std::string GetMatrixInfo(){return snapshot("matrix");}
std::string GetLuxSensor(){return snapshot("lux");}
std::string GetConfig(){return snapshot("config");}
struct Message {std::string topic,payload; bool retained;};
std::vector<Message> sent;
bool PublishMqtt(const char *topic,const char *payload,bool retained=false){
 assert(!mqttReceiveActive && client.buffer==256);
 sent.push_back({topic,payload,retained}); return true;
}
void SendWebSocketJson(unsigned,const std::string &payload){
 assert(!mqttReceiveActive && payload=="{\"sysinfo\":matrix}"); ++socketMessages;
}
void SendMatrixInfoTo(bool,bool);
''' + responses + timers + notifications + r'''
int main(){
 // Many updates during one receive retain only flags, never snapshots.
 mqttReceiveActive=true; client.buffer=6144;
 for(unsigned i=0;i<10000;++i) SendMatrixInfo();
 ServiceMqttResponses();
 assert(sent.empty() && snapshots==0 && socketMessages==0);
 assert(mqttPendingResponses==(MqttResponseMatrixPublish|MqttResponseMatrixWebsocket));
 mqttReceiveActive=false; client.buffer=256;
 ServiceMqttResponses();
 assert(snapshots==1 && sent.size()==2 && socketMessages==2 && mqttPendingResponses==0);
 assert(sent[0].topic=="master/matrixinfo" && sent[1].topic=="device/matrixinfo");
 assert(sent[0].retained && sent[1].retained && lastMqttMatrixInfoMillis==123);
 ServiceMqttResponses(); assert(snapshots==1 && sent.size()==2);
 // Every query keeps its topics, payload and non-retained delivery.
 sent.clear(); mqttPendingResponses=MqttResponseLux|MqttResponseMatrixQuery|MqttResponseConfig;
 ServiceMqttResponses(); assert(sent.size()==6 && snapshots==4);
 const char *suffixes[]={"luxsensor","matrixinfo","config"};
 const char *payloads[]={"lux","matrix","config"};
 for(unsigned i=0;i<6;++i){
  assert(!sent[i].retained && sent[i].payload==payloads[i/2]);
  assert(sent[i].topic==std::string(i%2?"device/":"master/")+suffixes[i/2]);
 }
 // MQTT and WebSocket destinations remain independent.
 sent.clear(); const unsigned before=socketMessages;
 mqttReceiveActive=true; SendMatrixInfoTo(false,true); mqttReceiveActive=false;
 ServiceMqttResponses(); assert(sent.empty() && socketMessages==before+2);
 mqttReceiveActive=true; SendMatrixInfoTo(true,false); mqttReceiveActive=false;
 ServiceMqttResponses(); assert(sent.size()==2 && socketMessages==before+2);
 // A disconnected broker cannot cause retries or an unbounded queue.
 sent.clear(); client.online=false; webSocket.viewers=0;
 mqttPendingResponses=31; const unsigned oldSnapshots=snapshots;
 ServiceMqttResponses(); assert(sent.empty() && snapshots==oldSnapshots && mqttPendingResponses==0);
 client.online=true; deviceTopics=false;
 mqttPendingResponses=MqttResponseLux; ServiceMqttResponses();
 assert(sent.size()==1 && sent[0].topic=="master/luxsensor");
 assert(rendered>0);
 uint32_t maximum=0;
 {MqttWorkTimer timer(maximum);now+=7;}assert(maximum==7);
 {MqttWorkTimer timer(maximum);now+=2;}assert(maximum==7);
 now=UINT32_MAX-2;{MqttWorkTimer timer(maximum);now+=11;}assert(maximum==11);
}
'''

sys.path.insert(0, str(root))
from transport_patches import patch_mqtt
original = next((root / '.pio/libdeps').glob('ESP8266*/PubSubClient/src/PubSubClient.cpp')).read_text()
patched = patch_mqtt(original)
handshake = patched[patched.index('            if (!write(MQTTCONNECT'):patched.index('            uint8_t llen;')]
handshake_program = r'''
#include <cassert>
#include <cstdint>
constexpr unsigned MQTTCONNECT=0x10,MQTT_MAX_HEADER_SIZE=5;
enum {MQTT_CONNECT_FAILED, MQTT_CONNECTION_LOST, MQTT_CONNECTION_TIMEOUT};
uint32_t tick=0,serviced=0;
uint32_t millis(){return tick;}
void ServiceTransportReceive(){++tick;++serviced;}
struct Wire {
 bool online=true;unsigned readyAt=UINT32_MAX,closes=0;
 bool connected(){return online;}
 unsigned available(){return online && tick>=readyAt;}
 void stop(){online=false;++closes;}
};
struct Client {
 Wire *_client;bool canWrite=true;unsigned socketTimeout=15;int _state=-1;
 uint8_t buffer[256]={};unsigned long lastInActivity=0,lastOutActivity=0;
 explicit Client(Wire &wire):_client(&wire){}
 bool write(unsigned,uint8_t*,unsigned){return canWrite;}
 bool connect(){unsigned length=10;
''' + handshake + r'''
 return true;}
};
int main(){
 Wire failed;Client cannotSend(failed);cannotSend.canWrite=false;tick=serviced=0;
 assert(!cannotSend.connect() && cannotSend._state==MQTT_CONNECT_FAILED);
 assert(tick==0 && serviced==0 && failed.closes==1);
 Wire closed;closed.online=false;Client disconnected(closed);tick=serviced=0;
 assert(!disconnected.connect() && disconnected._state==MQTT_CONNECTION_LOST && tick==1);
 Wire stalled;Client waiting(stalled);tick=serviced=0;
 assert(!waiting.connect() && waiting._state==MQTT_CONNECTION_TIMEOUT);
 assert(tick==1500 && serviced==1500 && stalled.closes==1);
 Wire shortWait;Client shorter(shortWait);shorter.socketTimeout=1;tick=serviced=0;
 assert(!shorter.connect() && tick==1000);
 Wire healthy;healthy.readyAt=50;Client success(healthy);tick=serviced=0;
 assert(success.connect() && tick==50 && serviced==50 && healthy.closes==0);
}
'''

mqtt = (root / 'src/Mqtt.cpp').read_text()
writer = mqtt[mqtt.index('size_t MqttWiFiClient::write(', mqtt.index('#if defined(ESP8266)')):mqtt.index('\n#endif', mqtt.index('#if defined(ESP8266)'))]
writer = writer.replace('unsigned long', 'uint32_t')
writer_program = r'''
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <string>
#include <algorithm>
uint32_t tick=0; unsigned serviced=0,displayCost=1;
uint32_t millis(){return tick;}
void delay(unsigned n){tick+=n;}
void ServiceDisplayFrames(){++serviced;tick+=displayCost;}
class WiFiClient {
public:
 uint32_t _timeout=5000; bool online=true,blocked=false,allocationFailure=false;
 int allowance=100000, roomLimit=100000; unsigned calls=0,closes=0;
 std::string wire;
 bool connected(){return online;}
 int availableForWrite(){return blocked?0:std::min(allowance,roomLimit);}
 void setTimeout(uint32_t timeout){_timeout=timeout;}
 size_t write(const uint8_t *p,size_t n){
  assert(n<=512 && n<=static_cast<unsigned>(availableForWrite()) && _timeout==1); ++calls;
  if(allocationFailure){tick+=2;return 0;}
  allowance-=n;wire.append(reinterpret_cast<const char*>(p),n);return n;
 }
 bool stop(unsigned wait){assert(wait==1);++closes;online=false;return true;}
};
class MqttWiFiClient:public WiFiClient{public:size_t write(const uint8_t*,size_t);};
''' + writer + r'''
int main(){
 const std::string packet(1800,'x');
 auto data=reinterpret_cast<const uint8_t*>(packet.data());
 MqttWiFiClient healthy; tick=serviced=0;
 assert(healthy.write(data,packet.size())==packet.size() && healthy.wire==packet);
 assert(healthy.calls==4 && healthy.closes==0 && healthy._timeout==5000 && serviced==4);
 MqttWiFiClient small; small.roomLimit=128; tick=0;
 assert(small.write(data,packet.size())==packet.size() && small.wire==packet && small.calls==15);
 MqttWiFiClient stalled; stalled.blocked=true; tick=serviced=0;
 assert(stalled.write(data,packet.size())==0 && tick-serviced*displayCost<=40 && serviced>10);
 assert(stalled.closes==1 && stalled.calls==0 && stalled._timeout==5000);
 MqttWiFiClient partial; partial.allowance=13; tick=serviced=0;
 assert(partial.write(data,packet.size())==13 && partial.wire==packet.substr(0,13));
 assert(partial.closes==1 && tick-serviced*displayCost<=40);
 assert(partial.write(data,packet.size())==0 && partial.wire.size()==13);
 MqttWiFiClient oom; oom.allocationFailure=true; tick=serviced=0; oom._timeout=77;
 assert(oom.write(data,packet.size())==0 && tick-serviced*displayCost<=43 && oom.closes==1 && oom._timeout==77);
 // Unsigned elapsed time keeps the write budget across millis() rollover.
 MqttWiFiClient rollover; rollover.blocked=true; tick=UINT32_MAX-10;serviced=0;
 const uint32_t start=tick; assert(rollover.write(data,packet.size())==0);
 assert(uint32_t(tick-start)-serviced*displayCost<=40 && rollover.closes==1);
 MqttWiFiClient empty;
 assert(empty.write(data,0)==0 && empty.write(nullptr,1)==0 && empty.closes==0);
 // A slow LED frame must not disconnect a healthy broker by consuming the
 // network wait budget. This reproduces a regression in the first candidate.
 MqttWiFiClient slowDisplay;displayCost=60;tick=0;
 assert(slowDisplay.write(data,packet.size())==packet.size());
 assert(slowDisplay.closes==0 && slowDisplay.wire==packet);
}
'''

# Run the actual MQTT loop block, so a later scheduling edit cannot move status
# construction or discovery back into the live receive-buffer/JSON lifetime.
runtime = (root / 'src/Runtime.cpp').read_text()
service = runtime[runtime.index('    if (mqttAktiv == true)', runtime.index('void loop()')):runtime.index('    ServiceButtons();', runtime.index('void loop()'))]
service_program = r'''
#include <cassert>
#include <cstdint>
bool mqttAktiv=true, mqttReceiveActive=false, poolAlive=false, discoveryDue=false;
unsigned long mqttLastReconnectAttempt=0,mqttLastService=0;
constexpr unsigned MQTT_RECONNECT_INTERVAL=5000,MQTT_IDLE_BUFFER_SIZE=256;
unsigned replies=0,discovery=0,frames=0,persisted=0,text=0,measured=0;
unsigned long millis(){return 10000;}
struct {bool incoming=true;int available(){return incoming?1:0;}} wifiClientMQTT;
struct {
 bool online=true;unsigned size=256,received=0;
 bool connected(){return online;}
 bool setBufferSize(unsigned n){size=n;return true;}
 void loop(){
  assert(mqttReceiveActive && size==6144);++received;
  struct Pool {Pool(){poolAlive=true;} ~Pool(){poolAlive=false;}} pool;
  assert(poolAlive);
 }
} client;
bool HADiscoveryDue(){return discoveryDue;}
bool PrepareMqttServiceBuffer(){return client.setBufferSize(6144);}
bool MQTTreconnect(){client.online=true;return true;}
void released(){assert(!mqttReceiveActive && !poolAlive && client.size==256);}
void ServiceDisplayFrames(){released();++frames;}
void ServiceMqttResponses(){released();++replies;}
void ServiceHADiscovery(){released();++discovery;}
void ServiceMqttConfig(){released();++persisted;}
void ServiceHAText(){released();++text;}
void RecordAnimationWork(const char*,unsigned long){released();++measured;}
void service(){
''' + service + r'''
}
int main(){
 service();assert(client.received==1 && replies==1 && discovery==1 && measured==1);
 client.online=false;service();
 assert(client.received==1 && replies==2 && discovery==2 && mqttLastReconnectAttempt==0);
 assert(frames==2 && persisted==2 && text==2);
 wifiClientMQTT.incoming=false;service();assert(replies==2);
 discoveryDue=true;service();assert(client.received==2 && replies==3 && discovery==3);
}
'''

with tempfile.TemporaryDirectory() as temp:
    path = Path(temp)
    for name, source in [('responses', program), ('writer', writer_program), ('service', service_program), ('handshake', handshake_program)]:
        (path / f'{name}.cpp').write_text(source)
        subprocess.run([shutil.which('clang++') or 'c++', '-std=c++11', '-DESP8266', '-Wall', '-Wextra',
                        '-fsanitize=undefined,bounds', str(path / f'{name}.cpp'),
                        '-o', str(path / name)], check=True)
        subprocess.run([str(path / name)], check=True)
print('Deferred MQTT snapshots, destination/retain policy, bounded writes, allocation pressure and rollover passed')

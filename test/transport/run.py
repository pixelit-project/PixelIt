"""Exercise actual patched MQTT packet reception and masked WebSocket compaction."""
from pathlib import Path
import subprocess
import sys
import tempfile
import shutil
root=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(root))
from transport_patches import patch_mqtt,patch_websocket
mqtt=next((root/'.pio/libdeps').glob('ESP8266*/PubSubClient/src/PubSubClient.cpp')).read_text()
mqtt=patch_mqtt(mqtt)
assert mqtt.count('if (MqttReceiveAccepted()) callback(')==2
ws=next((root/'.pio/libdeps').glob('ESP8266*/WebSockets/src/WebSockets.cpp')).read_text()
ws=patch_websocket(ws)
assert 'ReadCompactSocket' in ws
writer=ws[ws.index('size_t WebSockets::write(WSclient_t * client, uint8_t * out, size_t n)'):ws.index('size_t WebSockets::write(WSclient_t * client, const char * out)')]
packet=mqtt[mqtt.index('uint32_t PubSubClient::readPacket('):mqtt.index('boolean PubSubClient::loop()')]
source=(root/'src/TransportReceive.cpp').read_text()
receiver=source[source.index('unsigned ReadCompactSocket('):source.rindex('#endif')]
program=r'''
#include <cassert>
#include <string>
#include <vector>
#include <fstream>
#include <iterator>
#include <iostream>
#include <cstring>
#include "AnimationLimits.h"
#include "PixelItTransport.h"
using boolean=bool;
#define MQTTPUBLISH 0x30
#define MQTTQOS1 2
#define MQTT_DISCONNECTED -1
unsigned long tick=0;unsigned serviced=0,lastStatus=0;
unsigned long millis(){return tick;}
void ServiceTransportReceive(){++serviced;++tick;}
void RecordTransportReceive(bool,unsigned status,size_t,size_t){lastStatus=status;}
bool MqttReceiveAccepted(){return lastStatus==200;}
class WiFiClient {
public:
 std::string bytes;size_t offset=0;bool stalled=false,stopped=false;
 explicit WiFiClient(std::string bytes):bytes(bytes){}
 int available(){return stalled?0:bytes.size()-offset;}
 int read(){return offset<bytes.size()?static_cast<unsigned char>(bytes[offset++]):-1;}
 std::string written;
 int availableForWrite(){return tick<2?0:8;}
 size_t write(const uint8_t *data,size_t n){assert(n<=8);written.append(reinterpret_cast<const char*>(data),n);return n;}
 bool connected(){return !stopped&&(stalled||offset<bytes.size());}
 void stop(){stopped=true;}
};
class PubSubClient {
public:
 uint8_t storage[6144]={};uint8_t *buffer=storage;
 uint16_t bufferSize=6144;void *stream=nullptr;int _state=0;WiFiClient *_client;
 explicit PubSubClient(WiFiClient &c):_client(&c){}
 bool readByte(uint8_t *out){int c=_client->read();if(c<0)return false;*out=c;return true;}
 bool readByte(uint8_t *out,uint16_t *n){return readByte(out+(*n)++);}
 uint32_t readPacket(uint8_t *lengthLength);
};
'''
# stream.write is irrelevant to this application's null-stream receiver.
packet=packet.replace('this->stream->write(digit);','/* unused stream */')
program+=r'''
#define DEBUG_WEBSOCKETS(...)
#define WEBSOCKETS_TCP_TIMEOUT 5000
#define WEBSOCKETS_YIELD()
struct WSclient_t { WiFiClient *tcp; };
class WebSockets { public:size_t write(WSclient_t*,uint8_t*,size_t); };
'''+packet+receiver+writer+r'''
std::string encodeLength(size_t n){std::string s;do{unsigned c=n%128;n/=128;s+=static_cast<char>(c|(n?128:0));}while(n);return s;}
std::string message(const std::string &topic,const std::string &payload,bool qos){
 std::string body;body+=static_cast<char>(topic.size()>>8);body+=static_cast<char>(topic.size());body+=topic;
 if(qos){body+='\x12';body+='\x34';}
 body+=payload;return std::string(1,qos?'\x32':'\x30')+encodeLength(body.size())+body;
}
int main(int argc,char **argv){
 std::ifstream f(argv[1]);std::string fixture((std::istreambuf_iterator<char>(f)),{});
 for(bool qos:{false,true}){
  WiFiClient wire(message("unit/device/setScreen",fixture,qos));PubSubClient mqtt(wire);uint8_t llen;
  serviced=tick=lastStatus=0;size_t n=mqtt.readPacket(&llen);
  assert(n>0&&n<6144&&lastStatus==200&&wire.offset==wire.bytes.size()&&serviced>0);
  const size_t offset=llen+3+strlen("unit/device/setScreen");
  if(qos)assert(mqtt.buffer[offset]==0x12&&mqtt.buffer[offset+1]==0x34);
  DynamicJsonBuffer json;auto &screen=json.parseObject(reinterpret_cast<char*>(mqtt.buffer+offset+(qos?2:0)));
  assert(screen.success()&&IsValidBitmapAnimation(screen));
 }
 for(const auto &plain:{std::string("  hello  world  "),std::string("online"),std::string("A\0B",3),std::string("{ not JSON  ")}){
  WiFiClient wire(message("unit/setText",plain,false));PubSubClient mqtt(wire);uint8_t llen;
  size_t n=mqtt.readPacket(&llen),offset=llen+3+strlen("unit/setText");
  assert(lastStatus==200&&n-offset==plain.size()&&!memcmp(mqtt.buffer+offset,plain.data(),plain.size()));
 }
 for(auto pair:{std::make_pair(std::string(11300,' '),413u),std::make_pair(std::string("{\"probe\":\"")+std::string(10000,'x'),503u),std::make_pair(std::string("{\"bitmapAnimation\":{\"data\":[[65536]]}}"),406u)}){
  WiFiClient wire(message("unit/setScreen",pair.first,true));PubSubClient mqtt(wire);uint8_t llen;
  assert(mqtt.readPacket(&llen)>0&&lastStatus==pair.second&&!MqttReceiveAccepted());
  assert(wire.offset==wire.bytes.size());
 }
 WiFiClient tx("connected");WSclient_t channel{&tx};WebSockets sockets;tick=serviced=0;
 std::string outgoing(2000,'x');
 assert(sockets.write(&channel,reinterpret_cast<uint8_t*>(&outgoing[0]),outgoing.size())==outgoing.size());
 assert(tx.written==outgoing && serviced>=250);
 const uint8_t mask[4]={0x12,0x34,0xab,0xcd};std::string masked=fixture;
 for(size_t i=0;i<masked.size();++i)masked[i]^=mask[i%4];
 uint8_t output[6145];size_t used;WiFiClient ws(masked);serviced=tick=0;
 assert(ReadCompactSocket(ws,output,6144,masked.size(),mask,used)==200&&serviced>0&&used<6144);
 // Regression: the .40 device rejected a 6x256 gallery animation before
 // compaction because its largest block was below the old 6145-byte malloc.
 assert(Esp8266SocketReceiveCapacity(7848,5440)==5376);
 assert(Esp8266SocketReceiveCapacity(7848,4336)==4272);
 assert(Esp8266SocketReceiveCapacity(56,5440)==120);
 assert(Esp8266SocketReceiveCapacity(7848,64)==0);
 assert(Esp8266SocketReceiveCapacity(7848,20000)==6144);
 std::string alert="{\"setScreen\":{\"bitmapAnimation\":{\"size\":{\"width\":32,\"height\":8},\"data\":[";
 for(unsigned frame=0;frame<6;++frame){
  if(frame)alert+=",";alert+="[";
  for(unsigned pixel=0;pixel<256;++pixel){if(pixel)alert+=",";alert+=pixel%2?"65535":"0";}
  alert+="]";
 }
 alert+="]}},\"forcedDuration\":0}";
 const size_t smaller=Esp8266SocketReceiveCapacity(alert.size(),4336);
 std::vector<uint8_t> limited(smaller+1,0xaa);
 WiFiClient fragmented(alert);size_t packed=0;tick=0;
 assert(ReadCompactSocket(fragmented,limited.data(),smaller,alert.size(),nullptr,packed)==200);
 assert(fragmented.offset==alert.size() && packed<smaller && limited[smaller]==0xaa);
 limited[packed]=0;DynamicJsonBuffer alertJson;
 auto &envelope=alertJson.parseObject(reinterpret_cast<char*>(limited.data()));
 assert(envelope.success() && IsValidBitmapAnimation(envelope["setScreen"].as<JsonObject&>()));
 for(unsigned frame=0;frame<6;++frame){
  const char *encoded=envelope["setScreen"]["bitmapAnimation"]["data"][frame];
  uint16_t pixels[256];assert(encoded && DecodeAnimationPixels(encoded,strlen(encoded),pixels)==256);
  for(unsigned pixel=0;pixel<256;++pixel)assert(pixels[pixel]==(pixel%2?65535:0));
 }

 // Content that does not fit is drained and rejected, never truncated/applied.
 WiFiClient insufficient(alert);tick=0;
 assert(ReadCompactSocket(insufficient,limited.data(),128,alert.size(),nullptr,packed)==503);
 assert(insufficient.offset==alert.size());
 // Heap fragmentation can change between sizing and malloc; failure stays safe.
 WiFiClient allocationFailed(alert);tick=0;
 assert(ReadCompactSocket(allocationFailed,nullptr,smaller,alert.size(),nullptr,packed)==503);
 assert(allocationFailed.offset==alert.size());

 output[used]=0;DynamicJsonBuffer json;auto &screen=json.parseObject(reinterpret_cast<char*>(output));assert(screen.success()&&IsValidBitmapAnimation(screen));
 WiFiClient oom(masked);tick=0;assert(ReadCompactSocket(oom,nullptr,6144,masked.size(),mask,used)==503&&oom.offset==masked.size());
 WiFiClient tooBig(std::string(12289,' '));tick=0;assert(ReadCompactSocket(tooBig,output,6144,12289,nullptr,used)==413&&tooBig.offset==12289);
 WiFiClient stalled("x");stalled.stalled=true;tick=0;assert(ReadCompactSocket(stalled,output,6144,1,nullptr,used)==408);
 std::cout<<"MQTT QoS0/1 metadata, plain payloads, bounded compaction, rejection and masked WebSocket tests passed\n";
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.cpp').write_text(program)
 json_include=next((root/'.pio/libdeps').glob('*/ArduinoJson/src'))
 subprocess.run([shutil.which('clang++') or 'c++','-std=c++11','-DESP8266','-DMATRIX_WIDTH=32','-DMATRIX_HEIGHT=8','-I',str(root/'include'),'-I',str(json_include),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test'),str(root/'examples/animation-32x8.json')],check=True)

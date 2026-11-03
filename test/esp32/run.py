"""Host regression for the actual ESP32 HTTP/WebSocket batch receivers and limits."""
from pathlib import Path
import subprocess,tempfile,shutil
root=Path(__file__).resolve().parents[2]
http=(root/'src/HttpScreenUpload.cpp').read_text();http=http[http.index('void SampleHttpUploadMemory()'):http.rindex('#endif')]
ws=(root/'src/TransportReceive.cpp').read_text();ws=ws[ws.index('unsigned ReadCompactSocket('):ws.rindex('#endif')]
json_include=next((root/'.pio/libdeps').glob('*/ArduinoJson/src'))
program=r'''
#include <cassert>
#include <string>
#include <algorithm>
#include <iostream>
#include "AnimationLimits.h"
#include "PixelItTransport.h"
unsigned long tick=0;unsigned serviced=0,lastStatus=0;size_t allocationLimit=60000,peak=0;
unsigned long millis(){return tick;}
void yield(){++tick;}
void ServiceDisplayFrames(){++serviced;}
void ServiceTransportReceive(){++serviced;++tick;}
void RecordTransportReceive(bool,unsigned s,size_t,size_t){lastStatus=s;}
struct String {
 std::string value;
 bool reserve(size_t n){peak=std::max(peak,n);return n<=allocationLimit;}
 bool concat(char c){if(value.size()>=allocationLimit)return false;value+=c;return true;}
 size_t length()const{return value.size();}
 String &operator=(const char *p){value=p?p:"";return *this;}
};
struct WiFiClient {
 std::string input;size_t offset=0;bool stalled=false;
 explicit WiFiClient(std::string s):input(s){}
 int available(){return stalled?0:input.size()-offset;}
 bool connected(){return stalled||offset<input.size();}
 int read(uint8_t *out,size_t n){n=std::min(n,input.size()-offset);memcpy(out,input.data()+offset,n);offset+=n;return n;}
};
struct {uint32_t inputBytes=0,compactBytes=0,minHeap=0,minBlock=0;uint16_t status=0;} httpUploadStats;
using HttpUploadStats=decltype(httpUploadStats);
struct {uint32_t getFreeHeap(){return 200000;}uint32_t getMaxAllocHeap(){return 100000;}} ESP;
'''+http+ws+r'''
std::string animation(unsigned frames,unsigned width){
 std::string out="{\"bitmapAnimation\":{\"size\":{\"width\":"+std::to_string(width)+",\"height\":8},\"data\":[";
 for(unsigned i=0;i<frames;++i){if(i)out+=",";out+="[65535";for(unsigned j=1;j<width*8;++j)out+=",65535";out+="]";}
 return out+"]}}";
}
int main(){
 static_assert(MAX_ANIMATION_FRAMES==160 && MAX_ANIMATION_PIXELS==10240,"ESP32 profile");
 for(unsigned width:{8,16,32}){
  unsigned frames=MAX_ANIMATION_PIXELS/(width*8);std::string input=animation(frames,width);String body;WiFiClient client(input);
  assert(ReadScreenHttpBody(client,input.size(),body,5000)==200);
  assert(client.offset==input.size()&&serviced>100&&body.length()<SCREEN_RECEIVE_CAPACITY);
  DynamicJsonBuffer json;JsonObject &screen=json.parseObject(&body.value[0]);assert(IsValidBitmapAnimation(screen));
  std::string invalid=animation(frames+1,width);String rejected;WiFiClient bad(invalid);
  assert(ReadScreenHttpBody(bad,invalid.size(),rejected,5000)==200);
  DynamicJsonBuffer invalidJson;JsonObject &badScreen=invalidJson.parseObject(&rejected.value[0]);assert(!IsValidBitmapAnimation(badScreen));
  // Masking must use wire offsets across 128-byte batches.
  const uint8_t mask[]={1,22,33,44};std::string masked=input;for(size_t i=0;i<masked.size();++i)masked[i]^=mask[i%4];
  uint8_t output[SCREEN_RECEIVE_CAPACITY+1];WiFiClient socket(masked);size_t used=0;
  assert(ReadCompactSocket(socket,output,SCREEN_RECEIVE_CAPACITY,masked.size(),mask,used)==200);
  output[used]=0;DynamicJsonBuffer socketJson;JsonObject &socketScreen=socketJson.parseObject(output);assert(IsValidBitmapAnimation(socketScreen));
 }
 std::string boundary=animation(MAX_ANIMATION_PIXELS/256,32);boundary.resize(MAX_SCREEN_MESSAGE_SIZE,' ');String body;WiFiClient client(boundary);
 assert(ReadScreenHttpBody(client,boundary.size(),body,5000)==200);
 boundary+=' ';WiFiClient tooLarge(boundary);assert(ReadScreenHttpBody(tooLarge,boundary.size(),body,5000)==413&&tooLarge.offset==boundary.size());
 size_t used=0;WiFiClient tooLargeWs(boundary);assert(ReadCompactSocket(tooLargeWs,nullptr,0,boundary.size(),nullptr,used)==413&&tooLargeWs.offset==boundary.size());
 allocationLimit=1000;WiFiClient noMemory(animation(32,32));assert(ReadScreenHttpBody(noMemory,noMemory.input.size(),body,5000)==503&&noMemory.offset==noMemory.input.size());
 allocationLimit=60000;WiFiClient invalid("{\"bitmapAnimation\":{\"data\":[[65536]]}}");assert(ReadScreenHttpBody(invalid,invalid.input.size(),body,5000)==406);
 WiFiClient stalled("x");stalled.stalled=true;assert(ReadScreenHttpBody(stalled,1,body,5)==408);
 std::cout<<"ESP32 160/80/40 frame boundaries, masked batches, wire limits, rejection and render service passed\n";
}
'''
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);(p/'test.cpp').write_text(program)
 subprocess.run([shutil.which('clang++') or 'c++','-std=c++11','-fsanitize=undefined,bounds','-DESP32','-DMATRIX_WIDTH=32','-DMATRIX_HEIGHT=8','-I'+str(root/'include'),'-I'+str(json_include),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)

# Exercise the generated ESP32 status/ack writer with a stalled TCP socket.
import sys
sys.path.insert(0,str(root))
from transport_patches import patch_websocket
patched=patch_websocket(next((root/'.pio/libdeps').glob('ESP32*/WebSockets/src/WebSockets.cpp')).read_text(),esp32=True)
writer=patched[patched.index('size_t WebSockets::write(WSclient_t * client, uint8_t * out, size_t n)'):patched.index('size_t WebSockets::write(WSclient_t * client, const char * out)')]
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp)
 (p/'writer.cpp').write_text(r'''
#include <cassert>
#include <cstdint>
#include <cstddef>
#include <string>
#include <algorithm>
#include <sys/select.h>
int writable=1;
int select(int,fd_set*,fd_set*,fd_set*,timeval*){return writable;}
#define DEBUG_WEBSOCKETS(...)
#define WEBSOCKETS_TCP_TIMEOUT 5000
#define WEBSOCKETS_YIELD() (++tick)
#define MSG_DONTWAIT 1
#define SHUT_RDWR 2
int shutdown(int,int){return 0;}
unsigned long tick=0;int budget=10000;std::string wire;
unsigned long millis(){return tick;}
void ServiceTransportReceive(){++tick;}
void delay(unsigned long n){tick+=n;}
struct Tcp{bool connected(){return true;}int fd(){return 4;}};
struct WSclient_t{Tcp *tcp;uint8_t num=0;explicit WSclient_t(Tcp *t):tcp(t){}};
void RecordSocketWriteError(uint8_t,unsigned,int){}
class WebSockets{public:int disconnected=0;size_t write(WSclient_t*,uint8_t*,size_t);void clientDisconnect(WSclient_t *c){++disconnected;c->tcp=nullptr;}};
int send(int,const void *p,size_t n,int){size_t count=std::min(n,static_cast<size_t>(budget));budget-=count;wire.append(static_cast<const char*>(p),count);return count?count:-1;}
'''+writer+r'''
int main(){
 WebSockets ws;Tcp tcp;WSclient_t client{&tcp};std::string data(1551,'x');
 assert(ws.write(&client,reinterpret_cast<uint8_t*>(&data[0]),data.size())==data.size()&&wire==data);
 wire.clear();tick=0;budget=12;
 assert(ws.write(&client,reinterpret_cast<uint8_t*>(&data[0]),data.size())==12);
 assert(ws.disconnected==1&&client.tcp==nullptr&&tick<=5005);
 WSclient_t blocked{&tcp};wire.clear();tick=0;budget=10000;writable=0;
 assert(ws.write(&blocked,reinterpret_cast<uint8_t*>(&data[0]),data.size())==0);
 assert(wire.empty()&&blocked.tcp==nullptr&&tick<=5005);
}
''')
 subprocess.run([shutil.which('clang++') or 'c++','-std=c++11',str(p/'writer.cpp'),'-o',str(p/'writer')],check=True)
 subprocess.run([str(p/'writer')],check=True)
print('ESP32 generated status writer disconnects stalled/partially written messages within its service budget')

# Battery-less ESP32 must not touch ADC2/GPIO0; Ulanzi and ESP8266 keep sampling.
hardware=(root/'src/Hardware.cpp').read_text()
battery=hardware[hardware.index('void getBatteryVoltage()'):hardware.index('void SetCurrentMatrixBrightness(')]
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);(p/'battery.cpp').write_text(r'''
#include <cassert>
#include <cstdint>
constexpr int GPIO_NUM_34=34;
constexpr int MIN_BATTERY=255,MAX_BATTERY=665;
int batteryLevel=0;unsigned readings=0,pauses=0;
int analogRead(int pin){assert(pin==VBAT_PIN);++readings;return 500;}
void delay(unsigned n){pauses+=n;}
long map(long x,long low,long high,long a,long b){return (x-low)*(b-a)/(high-low)+a;}
'''+battery+r'''
int main(){
 getBatteryVoltage();
#if defined(ESP32)
 if(VBAT_PIN<=0){assert(readings==0&&pauses==0&&batteryLevel==0);return 0;}
#endif
 assert(readings==5&&pauses==5&&batteryLevel>1&&batteryLevel<100);
}
''')
 for flags in (['-DESP32','-DVBAT_PIN=0'],['-DESP32','-DVBAT_PIN=GPIO_NUM_34'],['-DESP8266','-DVBAT_PIN=0']):
  subprocess.run([shutil.which('clang++') or 'c++','-std=c++11',*flags,str(p/'battery.cpp'),'-o',str(p/'battery')],check=True)
  subprocess.run([str(p/'battery')],check=True)
print('Battery-less ESP32 avoids ADC2; Ulanzi enum pin and ESP8266 sampling remain unchanged')

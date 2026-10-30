"""Host regression tests for the actual firmware playback function and validator.
Run: python3 test/animation/run.py (requires clang++ or c++).
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'src/PixelIt.ino').read_text()
start = source.index('void AnimateBMP(bool isShowRequired)\n{')
end = source.index('\nvoid DrawSingleBitmap(', start)
publish_start = source.index('bool PublishMqtt(')
prepare_start = source.index('bool PrepareBitmapStorage()\n{')
prepare_end = source.index('\nvoid SendWebSocketJson(', prepare_start)
json_include = next((root / '.pio/libdeps').glob('*/ArduinoJson/src'))
program = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <iterator>
#include <memory>
#include <new>
#include "AnimationLimits.h"
std::unique_ptr<uint16_t[]> animationBmpList;
size_t animationBmpPixels=64;
std::unique_ptr<uint16_t[]> bmpArray;
size_t animationBmpCount;
unsigned long animationLastRenderedMillis=0, animationMaxFrameGap=0;
uint32_t animationRenderedFrames=0, animationFailedLedFrames=0;
unsigned long millis() { static unsigned long tick=0; return ++tick; }
int animateBMPCounter, animateBMPLimitLoops, animateBMPLoopCount;
bool animateBMPAktivLoop, animateBMPRubberbandingAktiv, animateBMPReverse;
int bmpPosX=0, bmpPosY=0, bmpWidth=8, bmpHeight=8;
std::vector<int> shown;
struct Matrix {
 void drawRGBBitmap(int,int,const uint16_t *pixels,int,int) { shown.push_back(pixels[1]); }
 void show() {}
} display;
Matrix *matrix = &display;
void ClearBMPArea() {}
#define MQTT_MAX_HEADER_SIZE 5
struct MqttClient {
 uint16_t size=256; bool connectedFlag=true, failResize=false, retained=false;
 int publishes=0;
 bool connected() { return connectedFlag; }
 uint16_t getBufferSize() { return size; }
 bool setBufferSize(uint16_t value) { if(failResize) return false; size=value; return true; }
 bool publish(const char *topic,const char *payload,bool retain) {
  assert(size>=5+2+strlen(topic)+strlen(payload));
  retained=retain; publishes++; return true;
 }
} client;
''' + source[publish_start:prepare_start] + source[prepare_start:prepare_end] + source[start:end] + r'''
void validate(int count, int pixels, bool expected, int firstPixel=2) {
 DynamicJsonBuffer buffer;
 JsonObject &screen=buffer.createObject();
 JsonObject &animation=screen.createNestedObject("bitmapAnimation");
 JsonArray &frames=animation.createNestedArray("data");
 for(int i=0;i<count;i++) {
  JsonArray &frame=frames.createNestedArray();
  for(int j=0;j<pixels;j++) frame.add(j==0 ? firstPixel : 65535);
 }
 assert(IsValidBitmapAnimation(screen)==expected);
 assert(PrepareBitmapAnimation(screen)==expected);
}
int main(int argc, char **argv) {
 assert(PublishMqtt("state","connected",true) && client.retained && client.size==256);
 std::string telemetry(2000,'x');
 assert(PublishMqtt("matrixinfo",telemetry.c_str()) && client.size==256);
 client.failResize=true;
 assert(!PublishMqtt("config",telemetry.c_str()) && client.size==256);
 client.failResize=false; client.size=MQTT_BUFFER_SIZE;
 assert(PublishMqtt("config",telemetry.c_str()) && client.size==MQTT_BUFFER_SIZE);
 std::string excessive(MQTT_BUFFER_SIZE,'x');
 assert(!PublishMqtt("config",excessive.c_str()));
 client.connectedFlag=false;
 assert(!PublishMqtt("state","connected"));

 for(int count : {1,2,9,10,20,28}) {
  validate(count,64,true);
  for(bool rubber : {false,true}) {
   animationBmpCount=count; animateBMPCounter=0; animateBMPLoopCount=0;
   animateBMPLimitLoops=2; animateBMPReverse=false;
   animateBMPAktivLoop=true; animateBMPRubberbandingAktiv=rubber; shown.clear();
   for(int i=0;i<count;i++) { memset(animationBmpList.get()+i*animationBmpPixels,0,animationBmpPixels*sizeof(uint16_t)); animationBmpList[i*animationBmpPixels]=2; animationBmpList[i*animationBmpPixels+1]=i; }
   for(int step=0;step<200 && animateBMPAktivLoop;step++) AnimateBMP(false);
   assert(!animateBMPAktivLoop);
   std::vector<int> expected;
   for(int loop=0;loop<2;loop++) {
    if(!rubber || count==1) { for(int i=0;i<count;i++) expected.push_back(i); }
    else {
     for(int i=loop==0 ? 0 : 1;i<count;i++) expected.push_back(i);
     for(int i=count-2;i>=0;i--) expected.push_back(i);
    }
   }
   assert(shown==expected);
  }
 }

 // Full matrix animations use the same total pixel storage as 8x8 icons.
 for(int count : {1,2,7,8}) {
  DynamicJsonBuffer buffer;
  JsonObject &screen=buffer.createObject();
  JsonObject &anim=screen.createNestedObject("bitmapAnimation");
  JsonObject &size=anim.createNestedObject("size"); size["width"]=32; size["height"]=8;
  JsonArray &frames=anim.createNestedArray("data");
  for(int i=0;i<count;i++) {
   JsonArray &frame=frames.createNestedArray();
   for(int j=0;j<256;j++) frame.add(i+10);
  }
  assert(IsValidBitmapAnimation(screen)==(count<=7));
  if(count>7) continue;
  assert(PrepareBitmapAnimation(screen));
  animationBmpPixels=256; animationBmpCount=count; bmpWidth=32; bmpHeight=8;
  for(int i=0;i<count;i++) for(int j=0;j<256;j++) animationBmpList[i*256+j]=i+10;
  animateBMPCounter=0; animateBMPLoopCount=0; animateBMPLimitLoops=1;
  animateBMPAktivLoop=true; animateBMPRubberbandingAktiv=false; animateBMPReverse=false; shown.clear();
  for(int step=0;step<count+1;step++) AnimateBMP(false);
  assert(!animateBMPAktivLoop && shown.size()==static_cast<size_t>(count));
  for(int i=0;i<count;i++) assert(shown[i]==i+10);
  for(int j=0;j<256;j++) assert(bmpArray[j]==count+9);
 }
 animationBmpPixels=64; bmpWidth=8; bmpHeight=8;
 // Boundary-sized CSV frames and rectangular dimensions.
 std::string fullCsv="65535"; for(int j=1;j<256;j++) fullCsv+=",65535";
 uint16_t fullPixels[256];
 assert(DecodeAnimationPixels(fullCsv.c_str(),fullCsv.size(),fullPixels)==256);
 assert(fullPixels[255]==65535);
 fullCsv+=",0"; assert(DecodeAnimationPixels(fullCsv.c_str(),fullCsv.size())==0);
 validate(0,64,false); validate(29,64,false);
 validate(1,63,false); validate(1,65,false);
 validate(1,64,false,-1); validate(1,64,false,65536);

 // Exercise the actual wire-format preprocessing with all 28 frames.
 std::string payload="{\"text\":{\"textString\":\"escaped \\\"bitmapAnimation\\\": text\"},\"bitmapAnimation\":{\"data\":[";
 for(size_t i=0;i<MAX_ANIMATION_FRAMES;i++) {
  if(i) payload+=",";
  payload+="[2";
  for(int j=1;j<64;j++) payload+=",65535";
  payload+="]";
 }
 payload+="],\"animationDelay\":100}}";
 std::vector<char> wire(payload.begin(),payload.end()); wire.push_back(0);
 assert(PackAnimationData(wire.data(),payload.size()));
 DynamicJsonBuffer packedBuffer;
 JsonObject &packed=packedBuffer.parseObject(wire.data());
 assert(packed.success() && IsValidBitmapAnimation(packed));
 assert(packedBuffer.size()<4096);
 JsonArray &packedFrames=packed["bitmapAnimation"]["data"].as<JsonArray>();
 assert(packedFrames.size()==MAX_ANIMATION_FRAMES);
 for(JsonVariant frame : packedFrames) {
  uint16_t pixels[64]; const char *csv=frame.as<const char *>();
  assert(csv && DecodeAnimationPixels(csv,strlen(csv),pixels)==64);
  assert(pixels[0]==2 && pixels[63]==65535);
 }

 // The browser sends a nested setScreen envelope, including optional whitespace.
 std::string wrapped="{\n \"forcedDuration\":0,\n \"setScreen\":"+payload+"}";
 std::vector<char> nested(wrapped.begin(),wrapped.end()); nested.push_back(0);
 assert(PackAnimationData(nested.data(),wrapped.size()));
 DynamicJsonBuffer nestedBuffer;
 JsonObject &envelope=nestedBuffer.parseObject(nested.data());
 assert(envelope.success());
 assert(IsValidBitmapAnimation(envelope["setScreen"]));
 assert(envelope["setScreen"]["bitmapAnimation"]["data"].size()==MAX_ANIMATION_FRAMES);
 assert(argc==2);
 std::ifstream fixtureFile(argv[1]);
 std::string fixture((std::istreambuf_iterator<char>(fixtureFile)),std::istreambuf_iterator<char>());
 assert(!fixture.empty());
 std::vector<char> actual(fixture.begin(),fixture.end()); actual.push_back(0);
 assert(PackAnimationData(actual.data(),fixture.size()));
 DynamicJsonBuffer fixtureBuffer;
 JsonObject &userMessage=fixtureBuffer.parseObject(actual.data());
 assert(userMessage.success() && IsValidBitmapAnimation(userMessage["setScreen"]));
 JsonArray &userFrames=userMessage["setScreen"]["bitmapAnimation"]["data"].as<JsonArray>();
 assert(userFrames.size()==14);
 for(JsonVariant frame : userFrames) {
  uint16_t pixels[64]; const char *csv=frame.as<const char *>();
  assert(DecodeAnimationPixels(csv,strlen(csv),pixels)==64);
 }
 for(const char *invalid : {"", "1,", "1, ", "-1", "65536", "01", "1,[2]", "1,1.5", "1,1e2"})
  assert(DecodeAnimationPixels(invalid,strlen(invalid))==0);
 for(const char *valid : {"2", " 2 ", "0,65535", "2,\n 65535"})
  assert(DecodeAnimationPixels(valid,strlen(valid))>0);
 char bad[]="{\"bitmapAnimation\":{\"data\":[[1,[2]]]}}";
 assert(!PackAnimationData(bad,strlen(bad)));
 animationBmpCount=0; animateBMPAktivLoop=true; shown.clear();
 AnimateBMP(false); assert(!animateBMPAktivLoop && shown.empty());
 animationBmpList.reset(); animationBmpCount=1; animateBMPAktivLoop=true;
 AnimateBMP(false); assert(!animateBMPAktivLoop);
 std::cout << "Animation validation and playback tests passed\n";
}
'''
with tempfile.TemporaryDirectory(prefix='pixelit-animation-') as tmp:
    cpp = Path(tmp) / 'test.cpp'
    binary = Path(tmp) / 'test'
    cpp.write_text(program)
    subprocess.run([shutil.which('clang++') or 'c++', '-std=c++11',
                    '-fsanitize=undefined,bounds', '-g', '-DMATRIX_WIDTH=32', '-DMATRIX_HEIGHT=8',
                    '-I' + str(root / 'include'), '-I' + str(json_include),
                    str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary), str(root / 'test/animation/webui_14_frames.json')], check=True)

# A blocked browser must never block physical LED playback via the preview.
with tempfile.TemporaryDirectory(prefix='pixelit-preview-') as tmp:
    folder = Path(tmp)
    (folder / 'WebSocketsServer.h').write_text(r'''
#include <cstddef>
#include <cstdint>
#include <string>
#include <string>
using String = std::string;
#define WSop_text 1
#define WSop_continuation 0
#define WEBSOCKETS_SERVER_CLIENT_MAX 2
#define WSC_CONNECTED 1
struct Tcp { int room=0; int availableForWrite() { return room; } };
struct Client { Tcp *tcp=nullptr; int status=0; };
class WebSocketsServer {
protected: Client _clients[2];
public:
 int sends=0; std::string wire; bool finished=false; int firstOpcode=-1;
 bool sendFrame(Client*,int op,uint8_t *data,size_t length,bool last) {
  if(sends==0) firstOpcode=op;
  wire.append(reinterpret_cast<char*>(data),length); sends++; finished=last; return true;
 }
 void disconnect(uint8_t) {}
 explicit WebSocketsServer(uint16_t) {}
 bool sendTXT(uint8_t,const char*,size_t) { sends++; return true; }
};
''')
    (folder / 'preview.cpp').write_text(r'''
#include <cassert>
#include "PixelItWebSockets.h"
class TestSocket : public PixelItWebSockets {
public:
 TestSocket():PixelItWebSockets(81) {}
 void connect(Tcp *tcp) { _clients[0].tcp=tcp; _clients[0].status=WSC_CONNECTED; }
};
int main() {
 TestSocket socket; Tcp tcp; std::string data(1551,'x');
 assert(!socket.sendPreview(0,data.c_str(),data.size()));
 assert(!socket.sendPreview(2,data.c_str(),data.size()));
 socket.connect(&tcp); socket.beginPreview(data.c_str(),data.size());
 tcp.room=0;
 assert(!socket.sendPreview(0,data.c_str(),data.size()) && socket.sends==0);
 tcp.room=1072;
 assert(!socket.sendPreview(0,data.c_str(),data.size()));
 assert(socket.wire.size()==1024 && !socket.finished && socket.firstOpcode==WSop_text);
 tcp.room=4;
 assert(!socket.sendPreview(0,data.c_str(),data.size()) && socket.sends==1);
 tcp.room=1072;
 assert(socket.sendPreview(0,data.c_str(),data.size()) && socket.finished);
 assert(socket.wire==data && socket.sends==2);
 assert(socket.sendPreview(0,data.c_str(),data.size()) && socket.sends==2);
 socket.beginPreview(data.c_str(),data.size());
 assert(!socket.sendPreview(0,data.c_str(),data.size()) && socket.sends==3);
 assert(socket.sendTXT(0,"OK",2) && socket.sends==5);
 assert(socket.finished && socket.wire==data+data);
 assert(socket.sendPreview(0,data.c_str(),data.size()) && socket.sends==5);
 socket.resetPreview(0);
 assert(!socket.sendPreview(0,data.c_str(),data.size()) && socket.sends==6);
}
''')
    binary = folder / 'preview'
    subprocess.run([shutil.which('clang++') or 'c++', '-std=c++11', '-DESP8266',
                    '-I' + str(folder), '-I' + str(root / 'include'),
                    str(folder / 'preview.cpp'), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
    print('Preview backpressure tests passed')

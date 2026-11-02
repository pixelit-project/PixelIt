"""Host checks for streaming screen compaction, bounded memory and rejection.
Uses the actual firmware receiver with deterministic WiFi/heap/timing stubs.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
source = (root / 'src/HttpScreenUpload.cpp').read_text()
source = source[source.index('void SampleHttpUploadMemory()'):source.rindex('#endif')]
json_include = next((root / '.pio/libdeps').glob('*/ArduinoJson/src'))
program = r'''
#include <cassert>
#include <string>
#include <fstream>
#include <iterator>
#include <iostream>
#include "AnimationLimits.h"
unsigned long tick=0;
unsigned displayCalls=0;
size_t maxAllocation=7432, allocated=0, peakAllocation=0;
struct String {
 std::string s; size_t capacity=0;
 bool reserve(size_t n) {
  if(n>maxAllocation) return false;
  capacity=n; allocated=n; if(n>peakAllocation) peakAllocation=n; return true;
 }
 bool concat(char c) {
  if(s.size()>=capacity && !reserve(s.size()+1)) return false;
  s+=c; return true;
 }
 size_t length() const { return s.size(); }
 String& operator=(const char *p) { s=p?p:""; if(!p) {capacity=0;allocated=0;} return *this; }
};
struct WiFiClient {
 std::string data; size_t offset=0; bool stalled=false; unsigned chunk=128;
 explicit WiFiClient(std::string s):data(s) {}
 int available() { return stalled?0:static_cast<int>(std::min<size_t>(chunk,data.size()-offset)); }
 int read() { return offset<data.size()?static_cast<unsigned char>(data[offset++]):-1; }
 bool connected() { return stalled || offset<data.size(); }
};
struct HttpUploadStats { uint32_t inputBytes=0,compactBytes=0,minHeap=0,minBlock=0;uint16_t status=0; } httpUploadStats;
struct Esp {
 uint32_t getFreeHeap() { return 20000-allocated; }
 uint32_t getMaxFreeBlockSize() { return 10000-allocated; }
} ESP;
unsigned long millis() { return tick; }
void yield() { ++tick; }
void ServiceDisplayFrames() { ++displayCalls; }
''' + source + r'''
std::string receive(const std::string &s,int expected=200) {
 String body; WiFiClient client(s); client.chunk=7;
 tick=displayCalls=allocated=peakAllocation=0;
 assert(ReadScreenHttpBody(client,s.size(),body,5000)==expected);
 assert(client.offset==s.size());
 assert(displayCalls>0 && peakAllocation<=maxAllocation);
 return body.s;
}
int main(int argc,char **argv) {
 std::ifstream file(argv[1]);
 std::string fixture((std::istreambuf_iterator<char>(file)),{});
 std::string compact=receive(fixture);
 assert(compact.size()<6144 && fixture.size()>10000);
 assert(peakAllocation==6144 && httpUploadStats.minBlock==3856);
 DynamicJsonBuffer before,after;
 auto &original=before.parseObject(fixture.c_str());
 auto &packed=after.parseObject(compact.c_str());
 assert(original.success() && packed.success() && IsValidBitmapAnimation(packed));
 for(size_t i=0;i<7;++i) {
  const char *encoded=packed["bitmapAnimation"]["data"][i];
  uint16_t pixels[256];
  assert(DecodeAnimationPixels(encoded,strlen(encoded),pixels)==256);
  for(size_t j=0;j<256;++j) assert(pixels[j]==original["bitmapAnimation"]["data"][i][j].as<unsigned>());
 }
 // All uint16 values, including single-character and three-character boundaries.
 for(unsigned value=0;value<=65535;++value) {
  const std::string raw="{\"bitmapAnimation\":{\"data\":[["+std::to_string(value)+"]]}}";
  std::string out=receive(raw);
  auto start=out.find('$'),end=out.find('"',start);
  uint16_t pixel;
  assert(DecodeAnimationPixels(out.data()+start,end-start,&pixel)==1 && pixel==value);
 }
 for(const auto &raw : {R"({"text":{"textString":"data [0] bitmapAnimation \\\""}})",
                       R"({"bitmap":{"data":[0,65535]}})",
                       R"({"other":{"data":[[1,2]]}})"}) assert(receive(raw)==raw);
 for(const auto &bad : {"[]","[1,]","[01]","[-1]","[65536]","[1.0]","[1 2]","[1e2]","[true]","[null]"})
  receive(std::string("{\"bitmapAnimation\":{\"data\":[")+bad+"]}}",406);
 receive("{\"bitmapAnimation\":{\"data\":[[1",406);
 receive(std::string(12289,' '),413);
 maxAllocation=100; receive(fixture,503); maxAllocation=7432;
 String body;WiFiClient slow("body");slow.stalled=true;tick=displayCalls=0;
 assert(ReadScreenHttpBody(slow,4,body,10)==408 && displayCalls>=10);
 WiFiClient truncated("{}");tick=0;
 assert(ReadScreenHttpBody(truncated,10,body,10)==400);
 assert(DecodeAnimationPixels("$A",2)==0);
 assert(DecodeAnimationPixels("$AA!",4)==0);
 std::cout<<"HTTP streaming, pixel fidelity, allocation rejection and render-service tests passed\n";
}
'''
with tempfile.TemporaryDirectory() as directory:
    folder = Path(directory)
    cpp = folder / 'test.cpp'
    cpp.write_text(program)
    binary = folder / 'test'
    subprocess.run([shutil.which('clang++') or 'c++', '-std=c++11',
                    '-DMATRIX_WIDTH=32', '-DMATRIX_HEIGHT=8', '-I', str(root / 'include'),
                    '-I', str(json_include), str(cpp), '-o', str(binary)], check=True)
    subprocess.run([str(binary), str(root / 'examples/animation-32x8.json')], check=True)

"""Actual ESP32 initial-state scheduler: ordering, backpressure and recovery."""
from pathlib import Path
import subprocess,tempfile,shutil
root=Path(__file__).resolve().parents[2]
source=(root/'src/SocketInitialState.cpp').read_text()
source=source[source.index('void ServiceSocketInitialState'):source.rindex('#endif')]
program=r'''
#include "PixelItSocketQueue.h"
#include <cassert>
#include <string>
#include <vector>
using String=std::string;
SocketOutputQueue socketOutput[5];
unsigned builds=0;bool online[5]={true,true,true,true,true},reject=false;
std::vector<std::string> messages[5];
String GetMatrixInfo(){++builds;return "1";}
String GetLuxSensor(){++builds;return "2";}
String GetSensor(){++builds;return "3";}
String GetConfig(){++builds;return "4";}
String GetButtons(){++builds;return "5";}
String GetTelemetry(){++builds;return "6";}
struct {
 bool clientIsConnected(uint8_t n){return online[n];}
 bool sendTXT(uint8_t n,const char *p,size_t size){
  if(reject)return false;
  assert(socketOutput[n].append(reinterpret_cast<const uint8_t*>(p),size));
  messages[n].emplace_back(p,size);return true;
 }
} webSocket;
''' + source + r'''
int main(){
 socketOutput[0].initialStep=1;socketOutput[1].initialStep=1;
 ServiceSocketInitialState();assert(builds==1&&messages[0].size()==1&&socketOutput[0].initialStep==2);
 // A full viewer must not block another; never build while its bytes remain.
 ServiceSocketInitialState();assert(builds==2&&messages[1].size()==1);
 ServiceSocketInitialState();assert(builds==2);
 for(int stage=2;stage<=6;++stage){
  for(int n=0;n<2;++n)socketOutput[n].consume(socketOutput[n].size());
  ServiceSocketInitialState();ServiceSocketInitialState();
 }
 assert(socketOutput[0].initialStep==0&&socketOutput[1].initialStep==0);
 assert(messages[0]==messages[1]&&messages[0].size()==6&&builds==12);
 assert(messages[0][0]=="{\"sysinfo\":1}"&&messages[0][3]=="{\"config\":4}"&&messages[0][5]=="{\"telemetry\":6}");
 socketOutput[2].initialStep=1;online[2]=false;
 ServiceSocketInitialState();assert(builds==12);
 online[2]=true;socketOutput[2].failed=true;ServiceSocketInitialState();assert(builds==12);
 socketOutput[2].reset();socketOutput[2].initialStep=1;reject=true;
 ServiceSocketInitialState();assert(socketOutput[2].initialStep==1);
 reject=false;ServiceSocketInitialState();assert(socketOutput[2].initialStep==2);
 // Disconnect cancels remaining startup state, even before any bytes were sent.
 socketOutput[2].reset();ServiceSocketInitialState();assert(socketOutput[2].initialStep==0);
}
'''
with tempfile.TemporaryDirectory() as tmp:
 p=Path(tmp);(p/'test.cpp').write_text(program)
 subprocess.run([shutil.which('clang++') or 'c++','-std=c++11','-I'+str(root/'include'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('ESP32 initial state: ordered snapshots, bounded work, blocked-viewer isolation and disconnect/retry passed')

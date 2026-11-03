"""Host regression for the actual bounded ESP32 socket output scheduler."""
from pathlib import Path
import subprocess, tempfile, shutil
root=Path(__file__).resolve().parents[2]
source=(root/'src/SocketOutput.cpp').read_text()
source=source[source.index('static_assert'):source.rindex('#endif')]
program=r'''
#include "PixelItSocketQueue.h"
#include <cassert>
#include <algorithm>
#include <string>
#include <errno.h>
#define WEBSOCKETS_SERVER_CLIENT_MAX 5
#define MSG_DONTWAIT 1
#define SHUT_RDWR 2
SocketOutputQueue socketOutput[5];
void ResetSocketOutput(uint8_t,bool enabled=false);
unsigned long tick=0;unsigned failures=0,services=0;int closed=-1;
std::string wire[5];bool blocked[5]={};size_t maxWrite=37;
unsigned long millis(){return tick;}
void ServiceTransportReceive(){++services;}
void RecordSocketWriteError(uint8_t,unsigned,int){++failures;}
int shutdown(int fd,int){closed=fd;return 0;}
int send(int fd,const void *data,size_t length,int){
 if(blocked[fd]){errno=EAGAIN;return -1;}
 size_t count=std::min(length,maxWrite);wire[fd].append(static_cast<const char*>(data),count);return count;
}
'''+source+r'''
int main(){
 std::string first(3000,'a'),second(2000,'b');
 assert(QueueSocketWrite(0,0,reinterpret_cast<const uint8_t*>(first.data()),first.size())==first.size());
 for(int i=0;i<60;++i){++tick;FlushSocketOutput();}
 assert(QueueSocketWrite(0,0,reinterpret_cast<const uint8_t*>(second.data()),second.size())==second.size());
 for(int i=0;i<150;++i){++tick;FlushSocketOutput();}
 assert(wire[0]==first+second&&socketOutput[0].size()==0&&SocketPreviewReady(0));
 // A stalled viewer never holds up another viewer or the main loop.
 blocked[0]=true;wire[0].clear();wire[1].clear();
 assert(QueueSocketWrite(0,0,reinterpret_cast<const uint8_t*>(first.data()),first.size())==first.size());
 assert(!SocketPreviewReady(0));
 assert(QueueSocketWrite(1,1,reinterpret_cast<const uint8_t*>(second.data()),second.size())==second.size());
 for(int i=0;i<100;++i){++tick;FlushSocketOutput();}
 assert(wire[1]==second&&wire[0].empty()&&failures==0);
 tick+=5001;FlushSocketOutput();assert(closed==0&&failures==1&&socketOutput[0].size()==0);
 // Overflow closes only that connection; never append a truncated message.
 maxWrite=1460;wire[2].clear();
 assert(QueueSocketWrite(2,2,reinterpret_cast<const uint8_t*>(first.data()),first.size())==first.size());
 assert(QueueSocketWrite(2,2,reinterpret_cast<const uint8_t*>(second.data()),second.size())==second.size());
 for(int i=0;i<10;++i)FlushSocketOutput();
 assert(wire[2]==first+second&&failures==1);
 blocked[2]=true;
 first.resize(SocketOutputQueue::capacity,'x');
 assert(QueueSocketWrite(2,2,reinterpret_cast<const uint8_t*>(first.data()),first.size())==first.size());
 assert(QueueSocketWrite(2,2,reinterpret_cast<const uint8_t*>(second.data()),1)==0);
 assert(closed==2&&failures==2&&socketOutput[2].size()==0);
 ResetSocketOutput(1);assert(socketOutput[1].fd==-1);
 assert(QueueSocketWrite(5,4,reinterpret_cast<const uint8_t*>(second.data()),1)==0);
 assert(!SocketOutputEnabled(1));ResetSocketOutput(1,true);assert(SocketOutputEnabled(1));
}
'''
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);(p/'test.cpp').write_text(program)
 subprocess.run([shutil.which('clang++') or 'c++','-std=c++11','-I'+str(root/'include'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('ESP32 output: partial writes, ring wrap, stalled-client isolation, bounded overflow and reconnect cleanup passed')

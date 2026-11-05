"""Actual asynchronous lwIP preview readiness: thread ownership and stale results."""
from pathlib import Path
import subprocess,tempfile,shutil
root=Path(__file__).resolve().parents[2]
source=(root/'src/SocketTransportProbe.cpp').read_text()
source=source[source.index('namespace'):source.rindex('#endif')]
program=r'''
#include "PixelItSocketQueue.h"
#include "PixelItSocketProbe.h"
#include <cassert>
#define _LIVEVIEW_BUFFER_LENGHT 1551
#define NETCONN_TCP 16
#define NETCONNTYPE_GROUP(t) (t)
#define ERR_OK 0
#define ESTABLISHED 4
SocketOutputQueue socketOutput[5];SocketTransportProbe socketTransportProbe;
int socketTransportProbeMux=0,locked=0;
bool onCore=false,allocate=true,post=true;
unsigned allocations=0,posts=0;uint32_t tick=20;
uint32_t millis(){return tick;}
void portENTER_CRITICAL(int*){assert(locked++==0);}
void portEXIT_CRITICAL(int*){assert(--locked==0);}
struct tcp_pcb{int state=ESTABLISHED;void *unacked=nullptr,*unsent=nullptr;unsigned snd_buf=5744;};
struct netconn{int type=NETCONN_TCP;struct{tcp_pcb *tcp;}pcb;};
struct lwip_sock{netconn *conn;void *lock=reinterpret_cast<void*>(1);};
#define pdTRUE 1
bool socketLocked=false,coreLockAvailable=true;
int xSemaphoreTake(void*,unsigned ticks){assert(onCore&&!locked&&ticks==0);if(!coreLockAvailable)return 0;assert(!socketLocked);socketLocked=true;return 1;}
void xSemaphoreGive(void*){assert(onCore&&socketLocked);socketLocked=false;}
tcp_pcb pcb[5];netconn conn[5];lwip_sock sockets[5];
lwip_sock *lwip_socket_dbg_get_socket(int fd){assert(onCore&&!locked);return fd<5?&sockets[fd]:nullptr;}
unsigned tcp_sndbuf(const tcp_pcb *p){return p->snd_buf;}
struct tcpip_callback_msg{void(*fn)(void*);};tcpip_callback_msg message;
tcpip_callback_msg *tcpip_callbackmsg_new(void(*fn)(void*),void*){assert(!locked&&!onCore);++allocations;message.fn=fn;return allocate?&message:nullptr;}
int tcpip_callbackmsg_trycallback(tcpip_callback_msg*){assert(!locked&&!onCore);++posts;return post?0:-1;}
void runCore(){assert(socketTransportProbe.pending);onCore=true;message.fn(nullptr);onCore=false;}
''' + source + r'''
int main(){
 for(int n=0;n<5;++n){conn[n].pcb.tcp=&pcb[n];sockets[n].conn=&conn[n];socketOutput[n].fd=n;}
 ServiceSocketTransportProbe();assert(allocations==1&&posts==1&&socketTransportProbe.pending);
 for(int n=0;n<20;++n)ServiceSocketTransportProbe();assert(posts==1);
 runCore();tick=30;ServiceSocketTransportProbe();assert(socketOutput[0].transportReady);
 // A queued/sent write after snapshot invalidates even a ready result.
 tick=40;ServiceSocketTransportProbe();++socketOutput[0].writes;socketOutput[0].transportReady=false;
 runCore();tick=50;ServiceSocketTransportProbe();assert(!socketOutput[0].transportReady);
 // Any unacknowledged or unsent byte, closed socket or insufficient room blocks previews.
 pcb[0].unacked=&pcb[0];pcb[1].unsent=&pcb[1];pcb[2].snd_buf=1554;pcb[3].state=0;
 tick=60;ServiceSocketTransportProbe();runCore();tick=70;ServiceSocketTransportProbe();
 for(int n=0;n<4;++n)assert(!socketOutput[n].transportReady);assert(socketOutput[4].transportReady);
 pcb[0].unacked=nullptr;tick=80;ServiceSocketTransportProbe();runCore();tick=90;ServiceSocketTransportProbe();assert(socketOutput[0].transportReady);
 // Native fd reuse cannot apply an old connection's core result.
 tick=100;ServiceSocketTransportProbe();socketOutput[0].reset();socketOutput[0].fd=0;
 runCore();tick=110;ServiceSocketTransportProbe();assert(!socketOutput[0].transportReady);
 // Busy core queue: no blocking, no second outstanding message or rapid retries.
 post=false;tick=120;ServiceSocketTransportProbe();assert(!socketTransportProbe.pending);
 unsigned before=posts;tick=121;ServiceSocketTransportProbe();assert(posts==before);
 post=true;tick=140;ServiceSocketTransportProbe();runCore();tick=150;ServiceSocketTransportProbe();assert(socketOutput[0].transportReady&&allocations==1);
 // Allocation failure is retried only once per second, outside critical sections.
 socketTransportProbe=SocketTransportProbe();allocate=false;tick=0;ServiceSocketTransportProbe();unsigned count=allocations;
 ServiceSocketTransportProbe();assert(allocations==count);
 for(int n=0;n<100;++n){++tick;ServiceSocketTransportProbe();}assert(allocations==count);
 allocate=true;tick=1000;ServiceSocketTransportProbe();assert(allocations==count+1);runCore();
 tick=1001;ServiceSocketTransportProbe();assert(!socketTransportProbe.pending);
 coreLockAvailable=false;tick=2020;ServiceSocketTransportProbe();runCore();tick=2021;ServiceSocketTransportProbe();
 assert(!socketOutput[0].transportReady&&!socketLocked);
 coreLockAvailable=true;
 // Millis rollover does not suspend polling.
 socketTransportProbe.lastPoll=0xfffffff0u;tick=10;ServiceSocketTransportProbe();assert(socketTransportProbe.pending);runCore();assert(locked==0);
}
'''
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);(p/'test.cpp').write_text(program)
 subprocess.run([shutil.which('clang++') or 'c++','-std=c++11','-fsanitize=undefined,bounds','-I'+str(root/'include'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('ESP32 transport probe: core-only PCB reads, ACK/queue readiness, stale write/fd rejection, bounded retries and rollover passed')

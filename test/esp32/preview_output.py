"""Exercise the real ESP32 preview policy with bounded fake socket output."""
from pathlib import Path
import subprocess, tempfile, shutil
root=Path(__file__).resolve().parents[2]
stub=r'''
#pragma once
#include <string>
#include <vector>
#include <utility>
#include <cstdint>
#define WEBSOCKETS_SERVER_CLIENT_MAX 5
#define WSC_CONNECTED 1
using String=std::string;
extern bool ready[5];
class WebSocketsServer {
public:
 explicit WebSocketsServer(uint16_t){}
 struct Client{int status=0;};
 Client _clients[5];
 std::vector<std::pair<unsigned,std::string>> sent;
 bool clientIsConnected(uint8_t n){return _clients[n].status==WSC_CONNECTED;}
 bool sendTXT(uint8_t n,const char *data,size_t length){sent.emplace_back(n,std::string(data,length));ready[n]=false;return true;}
};
'''
program=r'''
#include <cstdint>
uint32_t tick=0;
uint32_t millis(){return tick;}
#include "PixelItWebSockets.h"
#include <cassert>
#include <cstring>
bool ready[5]={true,true,true,true,true};
bool SocketPreviewReady(uint8_t n){return ready[n];}
PixelItWebSockets *activeServer;
bool SendSocketPreview(uint8_t n,const char *data,size_t length){return activeServer->sendTXT(n,data,length);}
int main(){
 PixelItWebSockets ws(81); activeServer = &ws; ws._clients[0].status=ws._clients[1].status=WSC_CONNECTED;
 char current[]="latest-a";ws.beginPreview(current,8);
 assert(ws.sendPreview(0,current,8));ready[1]=false;
 assert(ws.sendPreview(1,current,8));assert(ws.sent.size()==1);
 memcpy(current,"latest-b",8);ws.beginPreview(current,8);
 ready[1]=true;ws.flushSkippedPreviews();
 assert(ws.sent.size()==2&&ws.sent.back().first==1&&ws.sent.back().second=="latest-b");
 // Replace intermediate samples while the preceding frame awaits ACK.
 memcpy(current,"latest-c",8);ws.beginPreview(current,8);ws.sendPreview(1,current,8);
 memcpy(current,"latest-d",8);ws.beginPreview(current,8);ws.sendPreview(1,current,8);
 assert(ws.sent.size()==2);ready[1]=true;ws.flushSkippedPreviews();
 assert(ws.sent.size()==3&&ws.sent.back().second=="latest-d");
 ws.flushSkippedPreviews();assert(ws.sent.size()==3);
 ws._clients[1].status=0;ws.resetPreview(1);ws.flushSkippedPreviews();assert(ws.sent.size()==3);
 ws._clients[1].status=WSC_CONNECTED;ready[1]=true;ws.resetPreview(1);ws.flushSkippedPreviews();
 assert(ws.sent.size()==4);
 // Fast ACK-drained viewers have no four-FPS or shared time ceiling.
 PixelItWebSockets all(81); activeServer = &all;all.beginPreview(current,8);
 for(unsigned n=0;n<5;++n){all._clients[n].status=WSC_CONNECTED;ready[n]=true;all.sendPreview(n,current,8);}
 assert(all.sent.size()==5);
 for(unsigned round=0;round<12;++round){
  for(unsigned n=0;n<5;++n)ready[n]=true;
  all.beginPreview(current,8);
  for(unsigned n=0;n<5;++n)all.sendPreview(n,current,8);
 }
 assert(all.sent.size()==65&&tick==0);
 // A stalled/disconnected viewer cannot hold up an ACK-drained viewer.
 for(unsigned n=0;n<5;++n)ready[n]=false;
 all._clients[2].status=0;ready[4]=true;
 all.beginPreview(current,8);for(unsigned n=0;n<5;++n)if(n!=2)all.sendPreview(n,current,8);
 assert(all.sent.size()==66&&all.sent.back().first==4);
 // Replies are permitted while previews await ACK.
 all.sendTXT(0,"OK",2);assert(all.sent.size()==67&&all.sent.back().second=="OK");
 assert(!all.sendPreview(5,current,8));
}
'''
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);(p/'WebSocketsServer.h').write_text(stub);(p/'test.cpp').write_text(program)
 subprocess.run([shutil.which('clang++') or 'c++','-std=c++11','-DESP32','-I'+str(p),'-I'+str(root/'include'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('ESP32 previews: ACK readiness, no fixed FPS cap, stalled-viewer isolation, latest/static catch-up and replies passed')

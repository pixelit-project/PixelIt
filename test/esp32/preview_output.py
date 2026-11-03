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
class WebSocketsServer {
public:
 explicit WebSocketsServer(uint16_t){}
 struct Client{int status=0;};
 Client _clients[5];
 std::vector<std::pair<unsigned,std::string>> sent;
 bool clientIsConnected(uint8_t n){return _clients[n].status==WSC_CONNECTED;}
 bool sendTXT(uint8_t n,const char *data,size_t length){sent.emplace_back(n,std::string(data,length));return true;}
};
'''
program=r'''
#include "PixelItWebSockets.h"
#include <cassert>
#include <cstring>
bool ready[5]={true,true,true,true,true};
bool SocketPreviewReady(uint8_t n){return ready[n];}
int main(){
 PixelItWebSockets ws(81);ws._clients[0].status=ws._clients[1].status=WSC_CONNECTED;
 char current[]="latest-a";ws.beginPreview(current,8);
 assert(ws.sendPreview(0,current,8));ready[1]=false;
 assert(ws.sendPreview(1,current,8));assert(ws.sent.size()==1);
 // The shared sample may change while a viewer is backed up.
 memcpy(current,"latest-b",8);ws.beginPreview(current,8);
 ready[1]=true;ws.flushSkippedPreviews();
 assert(ws.sent.size()==2&&ws.sent[1].first==1&&ws.sent[1].second=="latest-b");
 // A final static sample must still arrive without a new checksum change.
 ready[1]=false;ws.sendPreview(1,current,8);ready[1]=true;
 ws.flushSkippedPreviews();assert(ws.sent.size()==3&&ws.sent.back().second=="latest-b");
 ws.flushSkippedPreviews();assert(ws.sent.size()==3);
 ws._clients[1].status=0;ws.resetPreview(1);ws.flushSkippedPreviews();assert(ws.sent.size()==3);
 ws._clients[1].status=WSC_CONNECTED;ws.resetPreview(1);ws.flushSkippedPreviews();
 assert(ws.sent.size()==4&&ws.sent.back().second=="latest-b");
}
'''
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);(p/'WebSocketsServer.h').write_text(stub);(p/'test.cpp').write_text(program)
 subprocess.run([shutil.which('clang++') or 'c++','-std=c++11','-DESP32','-I'+str(p),'-I'+str(root/'include'),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('ESP32 previews: skip busy viewers, catch up latest/static images and reconnect without duplicate pixel buffers passed')

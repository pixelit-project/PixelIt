"""Execute the actual per-viewer WebSocket subscription parser with ArduinoJson."""
from pathlib import Path
import subprocess,tempfile,shutil
root=Path(__file__).resolve().parents[2]
source=(root/'src/Api.cpp').read_text();a=source.index('            // Transient per-viewer subscription;');b=source.index('#endif',a);parser=source[a:b]
json_include=next((root/'.pio/libdeps').glob('ESP32*/ArduinoJson/src'))
program=r'''
#include <ArduinoJson.h>
#include "PixelItSocketQueue.h"
#include <cassert>
SocketOutputQueue socketOutput[5];
struct {unsigned resets=0;void resetPreview(uint8_t n){assert(n<5);++resets;}} webSocket;
void receive(uint8_t num,const char *payload){DynamicJsonBuffer buffer;JsonObject &json=buffer.parseObject(payload);
''' + parser + r'''
}
int main(){
 assert(socketOutput[0].previewEnabled);
 receive(0,"{\"preview\":{\"enabled\":false}}");assert(!socketOutput[0].previewEnabled&&webSocket.resets==1);
 assert(socketOutput[1].previewEnabled);
 for(auto bad:{"{}","[]","{\"preview\":false}","{\"preview\":{\"enabled\":0}}","{\"preview\":{\"enabled\":\"true\"}}","{\"preview\":{\"enabled\":true,\"extra\":1}}","{\"preview\":{\"enabled\":true},\"setScreen\":{}}"})receive(0,bad);
 assert(!socketOutput[0].previewEnabled&&webSocket.resets==1);
 receive(5,"{\"preview\":{\"enabled\":true}}");assert(webSocket.resets==1);
 receive(0,"{\"preview\":{\"enabled\":true}}");assert(socketOutput[0].previewEnabled&&webSocket.resets==2);
 receive(0,"{\"preview\":{\"enabled\":true,\"compact\":true}}");
 assert(socketOutput[0].compactPreview&&!socketOutput[1].compactPreview&&webSocket.resets==3);
 receive(0,"{\"preview\":{\"enabled\":true,\"compact\":1}}");
 assert(socketOutput[0].compactPreview&&webSocket.resets==3);
 receive(0,"{\"preview\":{\"enabled\":true,\"compact\":false}}");
 assert(!socketOutput[0].compactPreview&&webSocket.resets==4);
 socketOutput[0].previewEnabled=false;socketOutput[0].compactPreview=true;socketOutput[0].reset();
 assert(socketOutput[0].previewEnabled&&!socketOutput[0].compactPreview);
}
'''
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);(p/'test.cpp').write_text(program)
 subprocess.run([shutil.which('clang++') or 'c++','-std=c++11','-fsanitize=undefined,bounds','-I'+str(root/'include'),'-I'+str(json_include),str(p/'test.cpp'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
print('Preview subscription: exact booleans, viewer isolation, invalid input and legacy reconnect defaults passed')

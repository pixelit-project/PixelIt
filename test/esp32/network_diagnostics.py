"""Exercise retained WiFi events and MQTT timing through the firmware callbacks."""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
state = (root / 'include/FirmwareState.h').read_text()
state = state[state.index('struct NetworkDiagnostics'):state.index('extern NetworkDiagnostics')]
source = (root / 'src/NetworkDiagnostics.cpp').read_text()
source = source[source.index('// WiFi callbacks'):source.index('void AddNetworkDiagnostics')]
program = r'''
#include <cassert>
#include <cstdint>
#include <functional>
''' + state + r'''
NetworkDiagnostics networkDiagnostics;
int networkDiagnosticsMux = 0, lockDepth = 0;
void portENTER_CRITICAL(int*) { assert(lockDepth++ == 0); }
void portEXIT_CRITICAL(int*) { assert(--lockDepth == 0); }
uint32_t tick = 0;
uint32_t millis() { return tick; }
using WiFiEvent_t = int;
struct WiFiEventInfo_t { struct { uint8_t reason; } wifi_sta_disconnected; };
constexpr int ARDUINO_EVENT_WIFI_STA_DISCONNECTED = 1, ARDUINO_EVENT_WIFI_STA_GOT_IP = 2;
struct { std::function<void(int,WiFiEventInfo_t)> callback;
 void onEvent(std::function<void(int,WiFiEventInfo_t)> fn) { callback=fn; }
} WiFi;
struct { int value=0; int state() { return value; } } client;
''' + source + r'''
int main() {
 InitNetworkDiagnostics();
 tick=100; WiFi.callback(ARDUINO_EVENT_WIFI_STA_GOT_IP, {});
 tick=200; WiFi.callback(ARDUINO_EVENT_WIFI_STA_DISCONNECTED, {{201}});
 tick=300; WiFi.callback(ARDUINO_EVENT_WIFI_STA_GOT_IP, {});
 assert(networkDiagnostics.wifiDisconnects==1 && networkDiagnostics.wifiGotIP==2);
 assert(networkDiagnostics.wifiLastReason==201 && networkDiagnostics.wifiLastDisconnectMillis==200);
 assert(networkDiagnostics.wifiLastGotIPMillis==300);
 // Fast calls update maxima without erasing a retained slow event.
 tick=1249; RecordMqttDuration(false,1000); assert(networkDiagnostics.mqttSlowLoops==0);
 tick=1250; RecordMqttDuration(false,1000); assert(networkDiagnostics.mqttSlowLoops==1);
 client.value=-2; tick=2200; RecordMqttDuration(true,1000);
 assert(networkDiagnostics.mqttSlowConnects==1 && networkDiagnostics.mqttLastSlowWasConnect);
 assert(networkDiagnostics.mqttLastSlowState==-2 && networkDiagnostics.mqttLastSlowDuration==1200);
 tick=2300; RecordMqttDuration(false,2290);
 assert(networkDiagnostics.mqttLastSlowMillis==2200 && networkDiagnostics.mqttMaxLoopMillis==250);
 // Elapsed-time arithmetic must work across the device's 32-bit millis rollover.
 tick=100; RecordMqttDuration(false,0xffffff00UL);
 assert(networkDiagnostics.mqttSlowLoops==2 && networkDiagnostics.mqttMaxLoopMillis==356);
 assert(!networkDiagnostics.mqttLastSlowWasConnect && lockDepth==0);
}
'''
with tempfile.TemporaryDirectory() as temp:
    path = Path(temp)
    (path/'test.cpp').write_text(program)
    subprocess.run([shutil.which('clang++') or 'c++', '-std=c++11', str(path/'test.cpp'), '-o', str(path/'test')], check=True)
    subprocess.run([str(path/'test')], check=True)
print('ESP32 network diagnostics tests passed')

#!/usr/bin/env python3
"""Check actual configuration read/write fragments with absent and invalid values."""
from pathlib import Path
import subprocess
import tempfile
root=Path(__file__).resolve().parents[2]
source=(root/'src/Configuration.cpp').read_text()
setter=source[source.index('    if (json["wifiScreenAktiv"].is<bool>())'):source.index('    if (json.containsKey("bootBatteryScreen"))')]
writer='\n'.join(line for line in source.splitlines() if 'json["wifiScreenAktiv"] = wifiScreenAktiv;' in line)
assert setter and writer
program=r'''
#include <cassert>
#include <ArduinoJson.h>
bool wifiScreenAktiv=true;
void apply(JsonObject &json) {
''' + setter + r'''
}
void save(JsonObject &json) {
''' + writer + r'''
}
int main() {
    StaticJsonBuffer<2048> storage;
    JsonObject &old=storage.parseObject("{}");apply(old);assert(wifiScreenAktiv);
    JsonObject &off=storage.parseObject("{\"wifiScreenAktiv\":false}");apply(off);assert(!wifiScreenAktiv);
    apply(old);assert(!wifiScreenAktiv); // Partial/legacy writes preserve the choice.
    for(const char *bad:{"null","0","1","\"false\"","{}","[]"}) {
        StaticJsonBuffer<128> input;
        char message[80];snprintf(message,sizeof(message),"{\"wifiScreenAktiv\":%s}",bad);
        JsonObject &invalid=input.parseObject(message);apply(invalid);assert(!wifiScreenAktiv);
    }
    JsonObject &saved=storage.createObject();save(saved);
    assert(saved["wifiScreenAktiv"].is<bool>() && !saved["wifiScreenAktiv"].as<bool>());
    wifiScreenAktiv=true;apply(saved);assert(!wifiScreenAktiv);
    JsonObject &on=storage.parseObject("{\"wifiScreenAktiv\":true}");apply(on);assert(wifiScreenAktiv);
}
'''
with tempfile.TemporaryDirectory() as temp:
    path=Path(temp);(path/'config.cpp').write_text(program)
    lib=next((root/'.pio/libdeps').glob('*/ArduinoJson/src'))
    subprocess.run(['c++','-std=c++11','-Wall','-Wextra','-isystem',str(lib),str(path/'config.cpp'),'-o',str(path/'config')],check=True)
    subprocess.run([str(path/'config')],check=True)
print('WiFi screen boolean configuration tests passed')

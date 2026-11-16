"""Check physical FastLED pins across the old/new NodeMCU board macros."""
from pathlib import Path
import configparser
import shlex
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
headers = next((root / '.pio/libdeps').glob(
    'ESP8266*/FastLED/src/platforms/esp/8266/fastpin_esp8266.h')).parent
config = configparser.ConfigParser(interpolation=None)
config.read(root / 'platformio.ini')
nodemcu_flags = [flag for flag in shlex.split(
    config['env:ESP8266_nodemcuv2']['build_flags'])
    if flag.startswith('-DFASTLED_ESP8266_')]

program = r'''
#include <cassert>
#include <cstdint>
#define FASTLED_NAMESPACE_BEGIN
#define FASTLED_NAMESPACE_END
#define FASTLED_REGISTER
constexpr int OUTPUT=1, INPUT=0;
int selectedPin=-1;
uint32_t GP16O=0, GPO=0;
void pinMode(uint8_t pin, int mode) { assert(mode==OUTPUT); selectedPin=pin; }
template<uint8_t Pin> class FastPin;
#include "led_sysdefs_esp8266.h"
#include "fastpin_esp8266.h"
int main() {
    // Arduino's D2 constant is 4 on both boards. FastLED can remap it again.
    FastPin<4>::setOutput();
    assert(selectedPin==EXPECTED_GPIO);
    assert(FastPin<4>::mask()==(1U<<EXPECTED_GPIO));
}
'''
cases = (
    ('ARDUINO_ESP8266_NODEMCU', [], 2),
    ('ARDUINO_ESP8266_NODEMCU_ESP12E', nodemcu_flags, 2),
    ('ARDUINO_ESP8266_WEMOS_D1MINI', [], 4),
)
with tempfile.TemporaryDirectory() as directory:
    folder = Path(directory)
    source, binary = folder / 'pins.cpp', folder / 'pins'
    source.write_text(program)
    for board_macro, flags, gpio in cases:
        subprocess.run([shutil.which('clang++') or 'c++', '-std=c++11',
                        f'-D{board_macro}', f'-DEXPECTED_GPIO={gpio}', *flags,
                        '-I', str(headers), str(source), '-o', str(binary)], check=True)
        subprocess.run([str(binary)], check=True)
print('Legacy/current NodeMCU GPIO2 and D1 mini GPIO4 checks passed')

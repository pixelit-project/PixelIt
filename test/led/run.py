"""Exercise the pinned ESP8266 FastLED retry path with simulated interrupts.

Only the Xtensa cycle-counter instruction is replaced for host execution.
Requires FastLED installed by a PlatformIO build. Does not contact a device.
"""
from pathlib import Path
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[2]
driver = next((root / '.pio/libdeps').glob(
    'ESP8266*/FastLED/src/platforms/esp/8266/clockless_esp8266.h')).read_text()
start = driver.index('__attribute__ ((always_inline)) inline static uint32_t __clock_cycles()')
end = driver.index('#define FASTLED_HAS_CLOCKLESS', start)
driver = driver[:start] + 'inline uint32_t __clock_cycles() { return ++cycles; }\n' + driver[end:]

stub = r'''
#pragma once
#include <cstdint>
#include <cassert>
#include <vector>
#define FASTLED_NAMESPACE_BEGIN
#define FASTLED_NAMESPACE_END
#define FASTLED_REGISTER
#define IRAM_ATTR
#define CLKS_PER_US TEST_CPU_MHZ
#define C_NS(ns) (((ns)*CLKS_PER_US+999)/1000)
#define FASTLED_INTERRUPT_RETRY_COUNT 16
#define FASTLED_DEBUG_COUNT_FRAME_RETRIES
uint32_t cycles=0, _frame_cnt=0, _retry_cnt=0;
unsigned injectedGap=0, remainingInterrupts=0, highs=0;
std::vector<unsigned> waits;
enum EOrder { RGB, GRB };
template<int Pin> struct FastPin {
 using port_ptr_t=uint32_t*; using port_t=uint32_t;
 static void setOutput() {} static uint32_t mask() { return 1; }
 static uint32_t* port() { return nullptr; }
 static void hi() { ++highs; } static void lo() {}
};
template<int Wait> struct CMinWait {
 void wait() { waits.push_back(Wait); cycles+=Wait*CLKS_PER_US; }
 void mark() {}
};
template<EOrder Order> struct PixelController {
 int remaining=2;
 void preStepFirstByteDithering() {} void stepDithering() {}
 uint32_t loadAndScale0() { return 0; }
 uint32_t loadAndScale1() { return 0; }
 uint32_t loadAndScale2() { return 0; }
 uint32_t advanceAndLoadAndScale0() { --remaining; return 0; }
 bool has(int) { return remaining>0; }
};
template<EOrder Order> struct CPixelLEDController {};
void os_intr_lock() {}
void os_intr_unlock() {
 if (remainingInterrupts) { cycles+=injectedGap*CLKS_PER_US; --remainingInterrupts; }
}
void delayMicroseconds(unsigned us) { waits.push_back(us); cycles+=us*CLKS_PER_US; }
#include "driver.h"
'''
program = r'''
#ifdef TEST_BASELINE
#define FASTLED_ALLOW_INTERRUPTS 1
#define INTERRUPT_THRESHOLD 0
#include <FastLED.h>
template<uint8_t Pin> class PixelItLEDController : public ClocklessController<Pin,
 C_NS(250), C_NS(625), C_NS(375), GRB> {};
const unsigned reset=85, deadline=85;
#else
#include "PixelItLED.h"
const unsigned reset=300, deadline=40;
#endif
struct TestController : PixelItLEDController<4> {
 void send() { PixelController<GRB> pixels; showPixels(pixels); }
};
int main() {
 TestController controller;
 // A short ISR fits; a 55-us gap would latch older WS2812B prematurely.
 for (unsigned gap : {35u,55u,90u}) {
  cycles=highs=_frame_cnt=_retry_cnt=0; waits.clear();
  injectedGap=gap; remainingInterrupts=1;
  controller.send();
  assert(_frame_cnt==1);
  assert(_retry_cnt==(gap>deadline ? 1u : 0u));
  assert(waits.front()==reset);
  if (gap>deadline) { assert(waits.size()==2 && waits[1]==reset); assert(highs>48); }
 }
 // Continuous interference remains bounded; no completed frame is reported.
 cycles=highs=_frame_cnt=_retry_cnt=0; waits.clear();
 injectedGap=90; remainingInterrupts=100;
 controller.send();
 assert(_frame_cnt==0 && _retry_cnt==16);
 assert(highs==17*24);
 assert(waits.size()==17);
 for (unsigned wait : waits) assert(wait==reset);
}
'''
with tempfile.TemporaryDirectory() as directory:
    folder = Path(directory)
    (folder / 'FastLED.h').write_text(stub)
    (folder / 'driver.h').write_text(driver)
    (folder / 'test.cpp').write_text(program)
    binary = folder / 'test'
    for mhz in (80, 160):
        for baseline in (True, False):
            flags = ['-DTEST_BASELINE'] if baseline else []
            subprocess.run([shutil.which('clang++') or 'c++', '-std=c++11',
                            '-DESP8266', f'-DTEST_CPU_MHZ={mhz}', *flags,
                            '-I', str(folder), '-I', str(root / 'include'),
                            str(folder / 'test.cpp'), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)
print('ESP8266 LED interrupt deadline, reset and bounded retry checks passed')

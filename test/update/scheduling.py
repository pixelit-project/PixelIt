#!/usr/bin/env python3
"""Check the actual firmware notification scheduler on the host."""
from pathlib import Path
import subprocess
import tempfile
root = Path(__file__).resolve().parents[2]
source = (root / 'src/Runtime.cpp').read_text()
source = source[source.index('void SetAvailableFirmwareVersion('):source.index('void checkUpdate()')]
program = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include <string>
#define VERSION "1.0"
#define CHECKUPDATESCREEN_INTERVAL (30UL * 60 * 1000)
std::string lastReleaseVersion;
bool firmwareUpdateChecked = false, firmwareUpdateNotificationPending = false;
bool checkUpdateScreen = true, sleepMode = false;
uint32_t checkUpdateScreenPrevMillis = 0, tick = 31000;
int notifications = 0;
uint32_t millis() { return tick; }
int compareVersions(const char *a, const char *b) { return strcmp(a, b); }
void displayUpdateScreen() { ++notifications; }
''' + source + r'''
int main() {
    ServiceUpdateNotification();
    assert(notifications == 0); // No successful check yet.
    SetAvailableFirmwareVersion("2.0");
    ServiceUpdateNotification();
    assert(notifications == 1 && checkUpdateScreenPrevMillis == tick);
    ServiceUpdateNotification();
    assert(notifications == 1);
    tick += 100;
    SetAvailableFirmwareVersion("2.0"); // Same version is not rediscovered.
    ServiceUpdateNotification();
    assert(notifications == 1);
    tick = checkUpdateScreenPrevMillis + CHECKUPDATESCREEN_INTERVAL - 1;
    ServiceUpdateNotification(); assert(notifications == 1);
    ++tick;
    ServiceUpdateNotification(); assert(notifications == 2);
    SetAvailableFirmwareVersion("3.0");
    sleepMode = true;
    ServiceUpdateNotification(); assert(notifications == 2);
    assert(firmwareUpdateNotificationPending);
    sleepMode = false;
    ServiceUpdateNotification(); assert(notifications == 3);
    SetAvailableFirmwareVersion("4.0");
    checkUpdateScreen = false;
    ServiceUpdateNotification(); assert(notifications == 3);
    checkUpdateScreen = true;
    ServiceUpdateNotification(); assert(notifications == 4);
    SetAvailableFirmwareVersion("1.0");
    tick += CHECKUPDATESCREEN_INTERVAL;
    ServiceUpdateNotification(); assert(notifications == 4);
    assert(!firmwareUpdateNotificationPending);
    SetAvailableFirmwareVersion("0.9");
    ServiceUpdateNotification(); assert(notifications == 4);
    // Reminder interval remains wrap-safe across the 32-bit millis rollover.
    tick = UINT32_MAX - 100;
    SetAvailableFirmwareVersion("2.0");
    ServiceUpdateNotification(); assert(notifications == 5);
    tick += CHECKUPDATESCREEN_INTERVAL - 1;
    ServiceUpdateNotification(); assert(notifications == 5);
    ++tick;
    ServiceUpdateNotification(); assert(notifications == 6);
}
'''
with tempfile.TemporaryDirectory() as temp:
    path = Path(temp)
    (path / 'scheduling.cpp').write_text(program)
    subprocess.run(['c++', '-std=c++11', '-Wall', '-Wextra', str(path / 'scheduling.cpp'),
                    '-o', str(path / 'scheduling')], check=True)
    subprocess.run([str(path / 'scheduling')], check=True)
print('Firmware update notification scheduling tests passed')

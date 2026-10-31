# Firmware development

[Back to the README](../README.md) · [Animation usage](animations.md) · [Home Assistant](home-assistant.md)

## Building

Use PlatformIO with the environments in [platformio.ini](../platformio.ini).
Run commands from the repository root:

```sh
pio run -e ESP8266_d1_mini
```

Build every supported variant:

```sh
pio run -e ESP8266_generic -e ESP8266_d1_mini -e ESP8266_nodemcuv2 -e ESP32_generic -e ESP32_d1_mini32 -e ESP32_ulanzi
```

ESP8266 builds use espressif8266 4.2.1 / Arduino core 3.1.2 at 160 MHz,
with WiFi modem sleep disabled and additional FastLED transfer retries.
ESP32 builds use espressif32 6.5.0 for Arduino 2.0.14 compatibility.

## Source layout

The firmware uses ordinary C++ translation units; function declarations are explicit
and do not depend on Arduino sketch preprocessing.

| File | Responsibility |
| --- | --- |
| `src/Runtime.cpp` | Arduino `setup()` / `loop()`, scheduling and update checks |
| `src/Configuration.cpp` | Persist, load, apply and serialize device settings |
| `src/Api.cpp` | HTTP endpoints, WebSocket events and incoming MQTT messages |
| `src/Mqtt.cpp` | Broker connection and Home Assistant discovery |
| `src/Buttons.cpp` | Debounced button inputs and MQTT gesture events |
| `src/HAText.cpp` | Staged HA text settings and text rendering commands |
| `src/ScreenContent.cpp` | Interpret screen JSON and prepare display content |
| `src/Display.cpp` | Text, bitmap animation, clock and weekday rendering |
| `src/Effects.cpp` | Fades, transitions and boot/battery screens |
| `src/Hardware.cpp` | Battery readings, brightness and WiFi connection display |
| `src/HardwareConfig.cpp` | Pin/sensor translation, color correction and audio setup |
| `src/Sensors.cpp` | Sensor readings, device status and telemetry serialization |
| `src/Notifications.cpp` | Publish device changes and live previews |
| `src/TimeSync.cpp` | NTP requests and packet storage |
| `src/System.cpp`, `src/Logging.cpp` | Reset reason, version comparison and logging |
| `src/Tools.cpp`, `src/PixelItFont.cpp` | Utility implementations and font assets |
| `src/Liveview.cpp` | Preview buffer and delivery scheduling |

`include/Firmware.h` contains shared dependencies and build constants, including
`VERSION`, which the release scripts update. `include/FirmwareFunctions.h` declares
the internal function interfaces. `include/FirmwareState.h` declares shared state;
its storage and initial values live exclusively in `src/FirmwareState.cpp`.
The existing shared-state model is retained so this reorganization does not change
initialization order or device behavior. Add definitions to `.cpp` files and only
declarations to headers to avoid duplicate storage and implementations.

## Animation memory and scheduling

Limits are defined in [AnimationLimits.h](../include/AnimationLimits.h).
Increasing them requires checking both total free heap and the largest
contiguous free block on the target device.

Pixel arrays are preprocessed in place so ArduinoJson does not retain a node
for every pixel. Animation and snapshot storage are allocated lazily and reused.

The MQTT receive buffer grows before PubSubClient processes packets, then
returns to 256 bytes on ESP8266. It is never resized while a packet is being
read. Outgoing telemetry temporarily reserves the space required for its message.

[extra_script.py](../extra_script.py) prepares local copies of the pinned
ESP8266 HTTP server headers. The request body is moved instead of copied and
receive capacity is reserved in advance. The installed framework is unchanged.
Request storage is released before acknowledging a successful HTTP upload.

Blocking firmware checks and telemetry requests wait until animations and
scrolling text stop. Browser previews sample animations at up to 20 updates
per second. On ESP8266, previews use WebSocket fragments queued over multiple
loop iterations; the existing image buffer is preserved until delivery finishes.
Other JSON messages finish an active preview message first to preserve WebSocket
message ordering. Connection events reset per-client preview state.

## Host regression tests

After at least one PlatformIO build has installed ArduinoJson, run:

```sh
python3 test/animation/run.py
```

Requires Python 3 and a C++ compiler (`clang++` or `c++`). Tests cover the actual
firmware validator and playback function, frame boundaries, reverse playback,
8×8 and full-display limits, MQTT buffer resizing, WebUI envelopes, malformed
input and preview backpressure. The original 14-frame regression fixture is
included in [test/animation](../test/animation/).

## Physical-device timing test

Run only against a test ESP8266 with the current firmware:

```sh
python3 test/animation/device_timing.py http://192.168.0.40
```

Replace the address with your device's IP. This test replaces the current display
with a 32×8 animation for 60 seconds, checks frame timing and failed LED transfers,
and restores the clock afterwards. It does not change configuration.

## Recorded validation — 2026-10-30

- All six firmware variants compiled successfully.
- Seven 32×8 frames were verified pixel-for-pixel over HTTP, WebUI and MQTT.
- Switching back to 28 frames at 8×8 passed; oversized animations were rejected.
- The final ESP8266 D1 mini timing run completed 60 seconds with no dropped LED frames and a maximum frame interval of 154 ms at a configured 150 ms.
- Native animation and preview backpressure regression tests passed.

These results describe the tested device and run, rather than a timing guarantee
for every network or workload.

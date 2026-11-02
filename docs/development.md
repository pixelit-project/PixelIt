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

ESP8266 LED output uses `include/PixelItLED.h` with the pinned FastLED 3.7.0
driver, unchanged NEOPIXEL bit timing and GRB order. It waits 300 µs between
frames/retries, but aborts on inter-pixel interrupt gaps exceeding approximately
40 µs. FastLED couples these through `WAIT_TIME - INTERRUPT_THRESHOLD`; changing
reset time alone also changes the interruption deadline. Older WS2812B revisions
can latch after 50 µs low, whereas the original driver accepts gaps up to roughly
85 µs. A completed transfer counter cannot detect such premature receiver resets.
This is a candidate correction for the reported 2.6.0 → 2.7.0-beta.5 regression
on a 32×8 WS2812B matrix; the small reference matrix uses SK6805-EC15.
The precise WS2812B revision and causality on the physical device remain unverified.

The controller adds no pixel buffers or dynamic allocation. Regular interrupts
remain enabled between pixels; WiFi NMIs can still interrupt bits. The existing
16-retry limit is preserved. A nominal 256-pixel transfer takes about 7.68 ms;
17 attempts can still occupy roughly 136 ms plus interrupt/processing overhead.
The additional reset wait is at most 215 µs per attempt, and normally expires
between scheduled frames. Check actual jitter and retry rates under concurrent
HTTP/MQTT/WebSocket activity, rather than treating these nominal values as bounds.
ESP32 retains its existing NEOPIXEL driver. No payload, settings, endpoint or
companion repository changes are required.

On 2026-11-02, read-only `/api/matrixinfo` inspection of the reported large matrix
identified firmware 2.6.0, `ESP8266_d1_mini`, 80 MHz and 32×8 pixels. No firmware
was uploaded or display content changed during that initial inspection. This is baseline
identification; the subsequent authorized device run is recorded below. Before release, compare
the same content and brightness on 2.6.0, unmodified beta.5 and this correction;
check the WebUI preview against the physical pixels, static colors, animations,
and both LED types. Measure minimum heap/largest block under repeated concurrent
requests. Confirm retries, frame jitter and receiver output together: zero failed
frames alone does not establish correct LED colors or positioning.

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

The ESP8266 LED interruption policy has an additional host regression:

```sh
python3 test/led/run.py
```

It exercises the actual pinned driver at simulated 80/160 MHz, replacing only
the Xtensa cycle-counter access and hardware dependencies. It reproduces the old
acceptance of a 55 µs interrupt gap and checks the corrected deadline, 300 µs
reset and bounded retry exhaustion. It does not validate physical waveforms,
receiver behavior, WiFi ISR distributions or device heap.

Requires Python 3 and a C++ compiler (`clang++` or `c++`). Tests cover the actual
firmware validator and playback function, frame boundaries, reverse playback,
8×8 and full-display limits, MQTT buffer resizing, WebUI envelopes, malformed
input and preview backpressure. The original 14-frame regression fixture is
included in [test/animation](../test/animation/).

Release-channel and version comparison regressions can also be checked with:

```sh
python3 test/update/run.py
python3 test/homeassistant/run.py
```

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

## Recorded WS2812B validation — 2026-11-02

With explicit user authorization, the candidate D1 mini development build
(`0.0.0-beta`, 160 MHz) was installed on the reported 32×8 WS2812B matrix.
The user reported that the display looked correct. At the user's request,
`clockAutoFallbackActive` was persisted as `false`; the animation was left running.
Configuration saving also records the current automatically controlled brightness
and updates the stored firmware version.

- The first 60-second timing attempt was invalidated by the existing 25-second
  clock fallback: only 167 animation frames were rendered before the intended
  switch to the clock. This was not a passing timing run.
- After disabling fallback and waiting for its configuration-triggered reboot,
  the full 60-second run passed: 401 rendered frames, maximum interval 154 ms
  at a configured 150 ms, and zero failed LED frames. Retries did occur.
- Twenty repeated seven-frame, 32×8 HTTP animation uploads were then exercised
  with a simultaneous WebSocket preview receiver and the existing MQTT connection.
  All uploads completed; 314 complete preview messages were received without
  parse/WebSocket errors. Maximum sampled frame interval was 161 ms, with zero
  failed LED frames and no observed reboot. MQTT was connected at every sample.
- Across those upload samples, free heap was 16,768 bytes, and the smallest
  observed largest free block was 10,944 bytes. The first and last samples had
  the same free heap and largest block (11,488 bytes). These are sampled values,
  not instrumented allocation-time minima during request parsing or MQTT receive.
- Earlier Node.js-based concurrent test-client attempts received HTTP
  `ECONNRESET`; their load runs did not complete. The subsequent Python
  HTTP/WebSocket run above completed. The resets were not conclusively attributed
  to client or firmware and should be revisited in connection-churn testing.

All six board builds and LED-policy, animation/preview, Home Assistant and update
host tests passed before this device run. At the end of this run, remaining gaps included physical signal
capture, isolated A/B causality against unmodified beta.5, SK6805-EC15 device
regression testing, extended runtime/connection churn, and instrumented peak heap
with simultaneous large MQTT payloads. An active MQTT connection does not establish
large-payload MQTT memory safety. These short tests support the candidate on this
WS2812B device; they do not establish universal timing or memory guarantees.

## Recorded small-matrix comparison — 2026-11-02

The user subsequently authorized testing the small reference matrix (reported
SK6805-EC15, `ESP8266_d1_mini`, 32×8). It initially ran 2.7.0-beta.5 at 160 MHz.
The initial reset diagnostic already contained an exception from before these
tests; that historical value is not evidence of a new test-induced reboot.
Clock fallback was already disabled and no configuration settings were changed;
after installation only the stored firmware version differed in existing keys.

- An initial baseline attempt was invalidated by another client replacing the
  animation. After the user prepared the device, the complete 60-second baseline
  passed with 401 rendered frames, maximum interval 157 ms at 150 ms configured,
  and zero failed LED frames.
- The same 60-second test with the installed candidate (`0.0.0-beta`, 160 MHz)
  passed with 401 frames, maximum interval 152 ms and zero failed LED frames.
  The user also confirmed visually correct output. This short comparison shows
  no observed rendering regression; the 5 ms difference is not proof of a
  general performance improvement.
- The 20-upload, seven-frame 32×8 HTTP/WebSocket load scenario did not complete
  on either firmware. Beta.5 completed six uploads (maximum observed frame gap
  196 ms) before an HTTP connection reset on upload seven. The candidate's
  first upload received a connection reset. After that candidate failure the
  device was still running with uptime 161 seconds and software-restart status
  from the intentional OTA reboot, MQTT connected, 16,976 bytes free heap and
  largest free block 7,432 bytes (44% fragmentation). The full example is about
  10.9 KB. Insufficient contiguous space is a plausible cause, not an
  instrumented allocation-failure diagnosis. The different number of successful
  uploads does not establish equal load robustness or rule out a regression.
- A subsequent candidate-only run with seven-frame 8×8 icons completed all
  20 HTTP uploads alongside 309 complete WebSocket previews, with no preview
  parsing/socket errors, no failed LED frames and maximum frame interval 158 ms.
  MQTT stayed connected at every sample. Free heap remained 16,656 bytes and
  largest block 7,432 bytes at the first and last samples, with no sampled loss
  during this run. This validates the smaller workload, not maximum-size uploads
  or transient allocation-time minima. The candidate and test animation were
  left running; clock fallback remained disabled.

The large-upload failure predates the LED timing correction, but remains an
unresolved release-validation issue. Physical waveform capture, longer runs,
controlled connection churn and allocation-time memory measurements under
simultaneous large MQTT payloads are still outstanding.

## Update release channels

`includePrereleases` is an optional boolean setting, defaulting to `false`. Missing
fields in existing configurations preserve the current setting. With opt-in enabled,
version checks request `/api/lastversion?includePrereleases=true`; otherwise the
existing stable endpoint is unchanged. The API must be deployed with this query
support first. The response remains compact; release lists stay in the browser.
Changing the channel invalidates the cached result and schedules a new check using
the existing idle-only scheduler. No new polling or device buffers are introduced.
WebUI Options exposes the setting and uses it for release hints/download selection.
The WebUI must be rebuilt from its source repository before firmware packaging.
The workflow still references the legacy `pixelit-project/WebUI` repository;
GitHub redirects this to `pixelit-project/PixelIt.WebUI` (verified 2026-11-02).

External user documentation needs the option and its default documented when this
firmware reaches stable; PixelIt.Docs currently requires a stable-release baseline
and excludes unsolicited documentation of unreleased features. Node-RED payloads
and topics are unchanged. Device timing and ESP8266 peak-heap validation are still
required for release verification.

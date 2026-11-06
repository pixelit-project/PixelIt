# Firmware development

## Release documentation follow-up: logs

Before the next stable release, add a user-facing log reference to external
PixelIt.Docs. Explain MQTT/HTTP/WebSocket messages, component names, topic/command
fields, byte counts, receive status codes, normal versus error cases and useful
troubleshooting steps, with examples verified against the stable release tag.

For the current MQTT `Incoming JSON` log, `Bytes: A/B` prints the payload length
supplied to the application callback and `json.measureLength()` (the serialized
length of the parsed JSON object). These are not a direct wire/compacted-size
pair: transport compaction can already precede the callback, application packing
occurs before parsing, and JSON serialization can remove whitespace. Equal values
alone do not prove that no compaction occurred. Use receive diagnostics such as
`mqttReceive.inputBytes`/`compactBytes` when explaining transport compaction.

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
ESP32 builds use espressif32 6.5.0 for Arduino 2.0.14 compatibility and run
at 160 MHz to retain rendering headroom during larger screen commands. Both
platforms disable WiFi modem sleep to avoid DTIM/wakeup delays during playback.
ESP32 boards without a battery input also skip the unused GPIO0/ADC2 battery
poll; real battery inputs, including Ulanzi GPIO34, retain their existing sampling.
ESP32 power consumption and Ulanzi battery runtime need separate measurement.

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
| `src/HttpScreenUpload.cpp` | Bounded ESP8266 HTTP screen reception, pixel compaction and upload memory samples |
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

Limits are defined in [ScreenLimits.h](../include/ScreenLimits.h) and validated
by [AnimationLimits.h](../include/AnimationLimits.h).
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

For ESP8266 `POST /api/screen`, the local parser patch delegates the body to
`ReadScreenHttpBody`. Pixel values in `bitmapAnimation.data` are compacted as
they arrive into JSON-safe internal strings, before ArduinoJson sees the body.
All uint16 RGB565 values round-trip exactly. Values 0–47 take one character;
the rest take three. Seven 32×8 frames need at most 5,376 pixel characters plus
metadata rather than a roughly 10.9 KB decimal array. The receiver initially
reserves at most 6,144 bytes (less for small commands). Unusually large metadata
can require more; the bounded request may still be rejected when RAM is scarce.
No permanent request buffer or second animation pixel array is retained. The
existing animation is replaced only after the full JSON and dimensions validate
and pixel storage is available. MQTT and WebSocket wire formats remain unchanged.

The receiver processes at most 128 incoming bytes before servicing due animation
and scrolling frames and yielding to WiFi. It does not re-enter HTTP, MQTT or
WebSocket handlers during receipt; those application services resume afterwards.
An idle receive timeout is five seconds, with a ten-second total receive deadline.
The supported HTTP screen request limit is 12,288 wire bytes, matching the existing
screen-message budget. Rejected bodies are drained within those deadlines so
normal clients can receive JSON instead of a silent connection reset. Responses
include 413 for oversized payloads, 406 for invalid pixels/JSON, 503 for checked
request/JSON/pixel-storage allocation failure, and 408 for receive timeout.
Malformed or unavailable-memory requests preserve the existing display.

`/api/animationinfo` adds ESP8266 diagnostics `httpUploadInputBytes`,
`httpUploadCompactBytes`, `httpUploadMinHeap`, `httpUploadMinBlock` and
`httpUploadStatus` for the last HTTP screen request. Memory is sampled before and
after reserve, between receive batches, after JSON parsing and after commit.
These stage samples are not allocator-level instrumentation of every transient
allocation (including TCP internals and temporary strings).

Companion inspection: Node-RED's HTTP client already rejects non-success HTTP
statuses, and the WebUI uses the unchanged WebSocket screen envelope. No client
or PixelIt.API code changes are required. External `PixelIt.Docs/src/api.md` needs
the HTTP size/error behavior and optional diagnostics documented when this reaches
stable; its repository requires regular docs to describe stable releases, so this
unreleased behavior is recorded here rather than changing that 2.6.0 baseline.

Blocking firmware checks and telemetry requests wait until animations and
scrolling text stop. Browser previews sample animations at up to 20 updates
per second. ESP32 delivery waits for TCP acknowledgements before admitting another image
for a viewer; LED animation cadence is independent. On ESP8266, previews use WebSocket fragments queued over multiple
loop iterations; the existing image buffer is preserved until delivery finishes.
Other JSON messages finish an active preview message first to preserve WebSocket
message ordering. Connection events reset per-client preview state.

## Host regression tests

After at least one PlatformIO build has installed ArduinoJson, run:

```sh
python3 test/animation/run.py
```

For streaming compaction, exact RGB565 round-tripping, bounded allocations,
rejection/draining and rendering service during reception:

```sh
python3 test/http/run.py
```

The designated-device upload test replaces display content and leaves the test
animation active. It requires clock fallback to be disabled already; it does not
change configuration. It checks pixel-exact previews, repeated 32×8 uploads,
JSON errors, slow reception and continued playback during a rejected request:

```sh
python3 test/http/device_upload.py http://192.168.0.40 --uploads 60 --result /tmp/pixelit-upload-result.json
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

At this stage the large-upload failure predated the LED timing correction and was
an unresolved release-validation issue; the follow-up below addresses HTTP reception.
Physical waveform capture, longer runs,
controlled connection churn and allocation-time memory measurements under
simultaneous large MQTT payloads are still outstanding.

## Recorded HTTP upload correction — 2026-11-02

Both user-designated D1 mini devices were updated to the final upload correction
(`0.0.0-beta`, keeping the previously tested LED timing policy). All six board
variants built, and HTTP, animation/preview, LED policy, Home Assistant and update
host tests passed. The HTTP host suite covers all 65,536 RGB565 values and the
actual receiver's bounded reserve failure, invalid/truncated input, draining and
render-service calls. No framework/dependency version or animation limit changed.

The final designated-device runs used 60 repeated seven-frame 32×8 HTTP uploads
with a simultaneous WebSocket preview client and the existing active MQTT link:

| Final run | Small SK6805-EC15 | Large WS2812B |
| --- | ---: | ---: |
| Repeated large uploads completed | 60 | 60 |
| Complete preview messages | 572 | 575 |
| Distinct frames checked pixel-for-pixel | 7 | 7 |
| Pixel / preview protocol errors | 0 | 0 |
| Failed LED frames | 0 | 0 |
| Maximum reported frame interval (150 ms configured) | 156 ms | 151 ms |
| Minimum sampled heap during successful upload processing | 7,344 B | 7,288 B |
| Minimum sampled largest free block during successful upload processing | 3,904 B | 4,312 B |

The unpadded example occupied 10,910 wire bytes and 5,548 compact bytes. Valid
requests padded to exactly 12,288 bytes were accepted on both devices (5,549
compact bytes). Malformed pixel values returned JSON HTTP 406; oversized
12,289-byte bodies returned JSON HTTP 413, while preserving the running animation.
A deliberately large unterminated metadata string induced HTTP 503 on the small
device with its more fragmented heap, and HTTP 406 on the large device where the
body fit but JSON remained incomplete. Both delivered JSON errors without a TCP
reset, continued playback and accepted a subsequent valid request. The large
malformed probe sampled as low as 5,224 B heap / 1,832 B largest block; these are
separate from the successful-upload minima in the table.

Accepted and rejected requests were also sent in 256-byte chunks with 50 ms
pauses. During the slowly drained oversized request the prior animation advanced
by at least ten frames rather than stalling until reception completed. Reported
frame-gap counters reset on successful animation replacement, so the table is
not an independent continuous trace of every display transition. MQTT remained
connected in the sampled results; no dedicated large MQTT injection was made.
An earlier 60-upload run on the small device also completed before the final
small-command reserve refinement; the table describes the final binary's run.

After the tests, both devices accepted another valid large animation, reported
HTTP 200 / zero failed LED frames and remained reachable without an observed
unexpected reboot. The software-restart status came from the intentional OTA
updates. Both final builds and test animations were left running. No additional
configuration changes were made; clock fallback remained disabled as requested.

These results close the reproduced HTTP upload failure for the tested workloads.
Remaining release checks include longer stress/connection-churn runs, allocator-
level peak measurements, large simultaneous MQTT packets and physical signal
capture. Large WebSocket command reception is unchanged by this HTTP-specific
fix. Stage sampling and short runs do not establish an unlimited RAM/timing
guarantee or validate every client/network pattern.

## MQTT and WebSocket receive correction

ESP8266 builds now also generate local build-only copies of the pinned
PubSubClient 2.8.0 and WebSockets 2.4.1 translation units through guarded
`transport_patches.py` transformations. Installed library sources are not edited.
The generated object names retain `.cpp.o`, which the ESP8266 linker requires
for normal flash placement. Dependency/framework versions remain pinned.

MQTT screen payloads are compacted directly into the existing receive buffer,
including when the wire packet is larger than that buffer. The receive allocation
is 6,144 bytes, with the existing 2,048-byte small-control fallback; it shrinks to
256 bytes after service. The original 11,264-byte *wire packet* limit still
includes fixed header, topic and QoS message ID. Topic and message-ID offsets
are preserved. QoS1 rejected deliveries still receive the MQTT delivery PUBACK,
but do not reach application callbacks; PUBACK is not display acceptance.
Non-screen topics (especially literal HA text, including strings starting with
`{`) pass through byte-for-byte rather than being interpreted as JSON.
Outgoing MQTT capacity/limits and ESP32 buffering remain unchanged.

Complete text WebSocket messages are unmasked and compacted before the application
callback; browsers keep sending the same `setScreen` envelope. Initial capacity
is bounded to 6,144 bytes, with a smaller allocation for small messages, plus a
terminator. The previous 12,288-byte wire limit is unchanged. Invalid, oversized
and allocation-rejected messages are drained and receive a JSON `response` error
without replacing the current display. Incomplete/timed-out frames disconnect to
avoid treating remaining payload bytes as a new header. Control frames and the
library's existing fragmented/binary event behavior are retained; this does not
add application assembly of fragmented JSON commands.

The display scheduler is serviced during MQTT byte waits, body batches,
WebSocket header reads and TCP writes. WebSocket writes use at most 512 bytes
and never more than `availableForWrite()` before servicing rendering again.
These calls do not re-enter HTTP/MQTT/WebSocket parsing or update the preview
snapshot, preserving message ordering and the existing preview backpressure.

`/api/animationinfo` adds `mqttReceive` and `webSocketReceive` objects with
`messages`, `status`, `inputBytes`, `compactBytes`, `freeHeap` and `maxBlock`.
Status values 200/406/413/503 describe reception/compaction, not a new MQTT
application acknowledgement protocol. Heap/block values are receive-completion
samples while receive storage is held, not allocator-level minima during later
JSON interpretation. Ordinary WebSocket heartbeats do not overwrite command
diagnostics. No response topics, payload keys or client protocol changes are
needed. Node-RED and WebUI keep their existing transports. External PixelIt.Docs
needs these diagnostics/errors and unchanged wire limits described when stable;
its normal 2.6.0 documentation is intentionally not changed for this unreleased
firmware.

Host verification and explicitly designated-device tests:

```sh
python3 test/transport/run.py
python3 test/transport/device_upload.py http://192.168.0.40 --mode websocket --uploads 30 --result /tmp/pixelit-ws-result.json
python3 test/transport/device_upload.py http://192.168.0.40 --mode mqtt --uploads 30 --result /tmp/pixelit-mqtt-result.json
```

Device tests replace display content and leave an animation active. MQTT tests
require `mqttUseDeviceTopic` already enabled and use only that device's topic,
never the shared master screen topic. Restore the original setting afterwards.
Host tests exercise the actual patched MQTT packet reader (QoS0/1 and metadata),
raw HA text, rejection/callback suppression, masked WebSocket body reception,
allocation failure/timeouts and an actual patched WebSocket writer constrained
to an eight-byte simulated send window. Existing HTTP, animation/preview, LED,
Home Assistant (both receive-buffer policies) and update tests remain required.

## Recorded MQTT / WebSocket validation — 2026-11-02

Both designated devices were updated to the final transport build. For isolated
MQTT tests, their originally disabled `mqttUseDeviceTopic` setting was temporarily
enabled; publications used only each display's device topic. The shared master
screen topic was never used. Both original configurations were restored and
verified afterwards, excluding the expected stored firmware-version change.
Configuration-triggered restarts and OTA restarts were intentional.

The first WebSocket attempt was invalidated by another browser's heartbeat
replacing the last receive diagnostic. Heartbeats now leave command diagnostics
intact. A later small-device run completed uploads but failed the timing gate
with a 286 ms interval (228 ms earlier in that run). That was not a passing
run. Investigation identified TCP write calls that could wait without servicing
the display. After bounding writes to current send room and servicing display
work in read/write loops, the following final runs passed:

| Test (150 ms configured) | Small SK6805-EC15 | Large WS2812B |
| --- | ---: | ---: |
| WebSocket large commands | 30 | 30 |
| WebSocket complete previews | 301 | 304 |
| WebSocket maximum reported interval | 159 ms | 160 ms |
| MQTT QoS1 large commands | 30 | 30 |
| MQTT QoS1 complete previews | 329 | 335 |
| MQTT QoS1 maximum reported interval | 153 ms | 176 ms |
| Additional MQTT QoS0 commands | 5 | 5 |
| MQTT QoS0 maximum reported interval | 151 ms | 152 ms |
| Final HTTP regression uploads | 10 | 10 |
| Final HTTP maximum reported interval | 151 ms | 153 ms |

All seven full-display frames matched the expected gamma-expanded RGB565 pixels
in each run, with no observed pixel/protocol errors, transport disconnections or
failed LED frames in the checked upload samples. MQTT QoS1 reception reduced
10,910 payload bytes to 5,548 bytes; WebSocket reception reduced the 10,924-byte
command envelope to 5,562 bytes. Receive-completion snapshots across the 30-upload
runs reached minima of 10,408 / 10,608 bytes free heap for MQTT and 10,664 / 10,360
bytes for WebSocket (small / large). These snapshots hold receive storage but do
not measure every allocation during application JSON parsing. The final HTTP
regression also repeated pixel checks, the 12,288-byte boundary and error probes.

Both MQTT QoS levels and WebSocket rejected malformed pixels, oversized input
and an intentionally unfit large metadata string with diagnostic 406/413/503
statuses. WebSocket clients additionally received JSON response errors. Commands
were drained without invoking application callbacks on receiver rejection; prior
animation counters continued. Oversized requests sent in 256-byte chunks with
50 ms pauses advanced the existing animation by at least ten frames. MQTT
publisher PUBACK was treated only as broker delivery; tests waited for an
increased device receive counter and matching diagnostic to verify processing.

All six builds and HTTP, transport, animation/preview, LED, Home Assistant and
update host suites passed. Both devices were left on the final development build
with a valid animation, their original MQTT configuration and active broker links.
No unexpected configuration changes were found. These are short designated-device
runs; extended runtime/connection churn, simultaneous large packets from multiple
clients, allocator-level peak tracing, physical waveforms and fragmented JSON
command assembly remain outside this validation. The final measured maxima are
not a guarantee for every network or workload.

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

## ESP32 animation profile

`include/ScreenLimits.h` keeps platform-specific animation and transport budgets
in one place; `AnimationLimits.h` validates dimensions, frames and RGB565 values.
ESP8266 retains 28 frames / 1,792 pixels, 12,288 HTTP/WebSocket wire bytes,
11,264 MQTT packet bytes and the existing 6,144-byte compact receive allocation.
Its generated MQTT/WebSocket transformations are byte-identical to the previous
implementation with default arguments. No ESP8266 CPU or buffer limit increased.

ESP32 supports 160 frames / 10,240 pixels (160 at 8×8, 80 at 16×8, 40 at 32×8),
with 65,535-byte HTTP bodies/WebSocket commands and complete MQTT packets. The
RGB565 animation allocation is 20 KiB. Compact receive storage starts at at most
32 KiB; MQTT retains that buffer rather than resizing it between services. HTTP
can grow its checked String for larger metadata within the wire budget; MQTT
and WebSocket reject when compact content exceeds their receive capacity.
Limits are bounded profiles, not a promise of acceptance under arbitrary memory
pressure. PSRAM is not required or assumed.

ESP32 build-only guarded middleware patches the pinned WebServer `Parsing.cpp`
and PubSubClient/WebSockets translation units. HTTP screen bodies are compacted
during receipt and moved into server storage; a small server subclass exposes the
owned body for zero-copy JSON parsing. Multipart OTA and URL-encoded forms retain
the upstream parser. ESP8266's existing template/core patch is retained.
HTTP/WebSocket reads use at most 128 stack bytes before servicing rendering.
ESP32 accepted WebSocket sockets are marked nonblocking. Established sockets now
copy output bytes into a bounded 4 KiB ring per connected viewer, allocated lazily
and retained until disconnect (at most 20 KiB for the pinned five-client limit).
The main loop drains each ring with at most two nonblocking 1,460-byte sends per
iteration, servicing rendering between viewers. Enqueue-time bursts can make up to
three additional nonblocking send attempts on that viewer before rejecting an
otherwise oversized burst; no allocation growth or wait loop is introduced.
Frame headers, payloads and control
frames share the same ordered ring; temporary JSON strings are never retained by
reference. Handshakes retain the synchronous, rendering-aware writer. The queue is
enabled only after the initial handshake/ping and reset on connection lifecycle.

Replaceable previews are enqueued only when that viewer's ring is empty. A
per-viewer skipped flag (five bytes total) resends the latest shared Liveview sample
when the ring drains, including an unchanged final image and newly connected
viewers. There are no per-viewer pixel buffers. A viewer
which makes no send progress for five seconds is shut down without waiting in the
main loop; queue exhaustion/allocation failure closes only that viewer and records
status 503. Normal partial writes and EAGAIN retain their offset for a later loop.
There are no new pixel buffers and no growing or unbounded output allocations.
This adds up to 20 KiB to ESP32 peak use when all five viewers are active, alongside
the animation, compact input and MQTT buffers; verify peak heap and largest block
under representative repeated workloads. ESP8266 has no output rings and retains
its fragmented preview, TCP-room checks, wire limits and allocation budgets.

A new viewer receives its initial state only on its own socket. Reconnecting no
longer rebroadcasts configuration/system/sensor state to every viewer or forces
redundant MQTT updates. Existing message names, payloads and endpoints are unchanged;
no companion client update is required. PixelIt.WebUI already supports reconnects.
The external stable user documentation needs no new user-facing controls for this
transport correction.

ESP32 animation info includes `webSocketSend` with the last write timeout
(`failures`, `client`, `status`, `error`) to distinguish intentionally rejected
slow viewers from receive failures. MQTT outgoing packets retain their previous
11,264-byte limit, independently of the larger receive wire budget.

ESP32 matrix info adds `maxFreeBlock`, `minFreeHeap`, `wifiSleepMode` (zero for
disabled modem sleep), `maxScreenMessageSize` and
`maxMqttPacketSize`, and exposes the existing HTTP/transport receive diagnostics.
`minFreeHeap` is the allocator low-water mark since boot; receive stage samples
are not allocator-level minima or a complete trace across animation replacement.
`animationFailedLedFrames` is not instrumented on ESP32; zero does not establish
physical LED transfer success. The unchanged 150 ms test interval does not define
a guaranteed minimum playback interval for arbitrary content or load.

The companion WebUI Creator/Test Area now read advertised frame/pixel budgets,
with 28/1,792 fallback for older firmware and reset on disconnect. Source changes
live in PixelIt.WebUI and must be deployed with its normal build pipeline; the
embedded header is not manually edited. Node-RED and PixelIt.API retain their
existing JSON formats/topics; their local sources were inspected. PixelIt.Docs
needs the platform-specific limits, CPU setting, error behavior and diagnostics
at stable release. Its stable-baseline instructions exclude unsolicited prerelease
changes, so that external documentation remains unchanged for this development run.

Additional host and explicitly designated-device checks:

```sh
python3 test/esp32/run.py
python3 test/esp32/device_limits.py http://192.168.0.86 --result /tmp/pixelit-esp32-limits.json
python3 test/esp32/device_mqtt.py http://192.168.0.86 --result /tmp/pixelit-esp32-mqtt.json
```

The first device script changes display content and leaves a full-matrix animation.
The MQTT script also temporarily configures an isolated local broker and restores
its changed MQTT keys in `finally`, verifying them after restart. It only sends
to the designated device topic; it never publishes a shared screen topic. Both
scripts require clock fallback already disabled. MQTT tests need the device to
reach the test host on the temporary TCP port. Neither script establishes physical
waveforms, long-term stability, Ulanzi power consumption or all-board RAM safety.

## Recorded ESP32 validation — 2026-11-03

The user-designated 32×8 `ESP32_generic` at `192.168.0.86` initially ran
2.7.0-beta.7 at 80 MHz with 28 frames / 1,792 pixels. The final local test build
(`0.0.0-beta`) runs at 160 MHz, reports driver `wifiSleepMode: 0` and supports
160 / 80 / 40 frames at 8×8 / 16×8 / 32×8. No ESP8266 device was flashed.

All six board builds passed, as did animation/preview, HTTP, transport,
Home Assistant, LED-policy, update and the new ESP32 receiver/writer/battery host
tests. The ESP8266 generated MQTT/WebSocket transformations and preview header
were compared byte-for-byte with the starting revision and are unchanged.
WebUI tests/build passed; browser inspection confirmed device-advertised limits.
Import validation and reconnect fallback have additional Vue/store regressions.

Final designated-device results (150 ms animation delay):

| Check | Final result |
| --- | ---: |
| Maximum capacities accepted over HTTP | 160×8×8, 80×16×8, 40×32×8 |
| Alternating large HTTP / WebSocket uploads | 20 (10 each) |
| Pixel-exact full-matrix frames checked | 40 |
| Complete previews during HTTP/WebSocket run | 525 |
| Maximum reported interval across upload/timing samples | 166 ms |
| Maximum interval in continuous timing phase | 161 ms |
| Rendered frames in timing phase (at least 60 s) | 426 |
| SDK internal-heap low-water mark during that run | 112,096 B |
| Minimum sampled HTTP-stage largest internal block | 59,380 B |
| Large MQTT QoS0 / QoS1 commands | 15 / 15 |
| Concurrent HTTP uploads during MQTT test | 20 |
| Complete previews during MQTT test | 136 |
| Maximum reported MQTT/error-probe interval | 171 ms |
| SDK internal-heap low-water mark during MQTT run | 109,512 B |
| Minimum sampled HTTP-stage largest internal block with MQTT active | 47,092 B |

The generated 40-frame full-display payload occupied 61,627 wire bytes and 30,987
compact bytes. HTTP bodies and WebSocket envelopes padded to exactly 65,535 bytes
were accepted; 65,536-byte messages were rejected. MQTT packets padded to exactly
65,535 bytes, including topic/header/QoS overhead, passed for both QoS levels.
Malformed pixels, oversized packets and a compact metadata string exceeding
receive storage produced 406 / 413 / 503 receive diagnostics. A slowly drained
rejected request advanced the existing animation instead of resetting counters.
The MQTT run completed 37 deliveries including boundaries/errors and a final valid
animation; PUBACK was checked separately from receive diagnostics and pixel output.

The HTTP/WebSocket timing phase deliberately left a command viewer unread. Its
write timed out after the existing approximately five-second deadline (one expected
`webSocketSend` failure); rendering continued and the actively reading preview
client stayed connected, with no observed pixel or protocol errors. The five-second
task duration is not a five-second animation stall. Application services can still
wait behind a blocked write until that deadline. Statistics reset on successful
animation replacement; these samples are not a continuous hardware-level trace
across every replacement or a physical LED waveform/failed-transfer measurement.

Earlier experimental candidates did not pass: some timed out or disconnected
viewers, and timing outliers exceeded the final gate. The final profile combines
CPU, WiFi, transport and unused-ADC corrections; these runs do not isolate the
causal contribution of each correction. An initial final-code MQTT load attempt
with two independent HTTP test clients timed out. The successful rerun serialized
HTTP uploads/diagnostic requests through the firmware's single HTTP handler and
fully drained responses, while MQTT and WebSocket activity remained concurrent.
It does not establish robustness under unrestricted parallel HTTP connections.

MQTT settings were temporarily pointed at an isolated local broker; only the
specified device topic was used. They were restored and verified after each run.
Saving configuration had also persisted the generated hostname where the original
file contained an empty hostname; the original empty value was restored explicitly.
A full final configuration comparison found only the expected stored firmware
version difference. Clock fallback remained disabled, and the device was left on
the final build with a valid 40-frame animation. Configuration/OTA restarts were
intentional; no unexpected restart was observed in the successful runs.

These are short tests on the generic ESP32, not a universal hardware maximum or a
release guarantee for every board/workload. Remaining checks include long-duration
stress/connection churn, unrestricted multiple HTTP clients, simultaneous large
commands on all transports, allocation-time tracing of 8-bit/DMA-capable pools,
physical LED signal capture, Ulanzi battery/power measurements and device tests of
the other ESP32 variants. ESP8266 received builds/host regressions in this task;
its prior physical-device results remain historical rather than a new run.


### Gallery compatibility in the companion WebUI

The companion PixelIt.WebUI Gallery uses the advertised screen dimensions, bitmap
budget, animation frame/pixel budgets and message limit. It defaults to compatible
entries, offers an option to show oversized entries with disabled Send and an
explicit reason, and displays dimensions and real frame counts. Offline browsing
and previews remain available. Disconnect clears capabilities; sending waits for
fresh system information and revalidates the payload. Legacy firmware falls back
to 28 frames / 1,792 pixels. No board-name assumptions or automatic frame trimming
are used. The API's existing frame strings supply metadata without a schema change;
only scalar metadata is cached and full pixel parsing occurs on send.

WebUI host tests, targeted lint and the production build passed. Browser checks
with isolated mock ESP8266/ESP32 profiles covered filtering, oversized reasons,
disabled sends, offline browsing and reconnecting to smaller limits. These checks
sent no commands to a physical display. Firmware is unchanged by this Gallery
work; no new physical ESP8266 heap or playback-timing measurements were performed.
The release integration must publish the companion WebUI and update the Gallery
user guide in PixelIt.Docs when the behavior reaches a stable release. The embedded
Webinterface header remains generated build output and was not edited manually.


### WebUI disconnection investigation

Read-only inspection of the ESP32 at 192.168.0.86 (2.7.0-beta.8, 160 MHz,
modem sleep disabled) found five WebSocket write timeouts. An actively reading
observer was reset after 54 seconds; the timeout counter rose to eight, while
uptime continued without a reboot. A second 90-second observer completed while
other connections timed out (counter ten). Free heap remained approximately
165–171 KiB with a 90,100-byte largest block; this was not an allocation-exhaustion
observation. These are baseline observations, not measurements of the correction.
The previous writer serviced rendering but could still hold other networking work
behind each five-second no-progress deadline.

Host regression `python3 test/esp32/socket_output.py` exercises actual output
scheduler code with partial writes, ring wrap, a blocked viewer beside a reading
viewer, overflow rejection and reconnect cleanup. `test/esp32/run.py` also checks
that established sockets enqueue without waiting and handshakes keep their bounded
writer. Device results for the correction must be recorded separately below.


The first queued-output candidate kept two reading viewers connected for 154 seconds
(355 and 347 complete preview messages) beside a deliberately unread viewer and
repeated connection churn. A later four-viewer check exposed initial-state burst
exhaustion; the bounded enqueue-time drain corrects that case without enlarging the
ring. Four simultaneous reading test viewers plus the existing WebUI then completed
30 native Ping/Pong exchanges each without disconnect or malformed JSON. Their
maximum round-trip times were 2.204–2.227 seconds; this establishes connection
survival, not uniformly low interaction latency. Heap samples were 136,264–140,608
bytes, minimum-since-boot 113,196 bytes and sampled largest block at least 81,908
bytes. Existing MQTT traffic still accounts for approximately one-second task
outliers and frame-gap diagnostics around 0.5 seconds in this workload.

The user confirmed that the WebUI was stable again while operating it. The final
candidate also includes the five-byte per-viewer latest-preview retry flags, covered
by `python3 test/esp32/preview_output.py` for static-image catch-up and reconnects.
All six boards build; animation/preview, HTTP, MQTT transport, Home Assistant and
the ESP32 output/receiver host regressions pass. The generated ESP8266 WebSocket
translation unit remains byte-identical to the previous implementation. No new
ESP8266 physical timing/heap tests were performed. Long-duration stress, maximum
animation/upload workloads with all viewers, and physical LED waveform timing
remain outstanding; these short tests do not establish universal jitter or RAM
safety. The device was intentionally restarted for candidate OTA installs, with
no configuration changes other than the normal stored build-version update.


Final-code backpressure verification used a socket with a 1 KiB receive buffer
configured before connecting, then intentionally stopped reading. That client alone
was shut down with the expected status 408/EAGAIN. The parallel reading viewer
completed all 30 native Ping/Pong exchanges, no JSON/connection error, maximum
round-trip 0.902 seconds. The user's existing WebUI remained usable. Final read-only
checks found only the stored firmware-version configuration change; the temporary
configuration backup was removed. Uptime 69 seconds versus 70 seconds since the
last OTA response, with reset reason Software reset, identified the observed reboot
as the intentional final OTA restart rather than an additional unexpected reset.

## MQTT sensor publishing policy (unreleased)

`mqttSensorPublish` is an optional configuration object shared by HTTP/MQTT/
WebSocket configuration writes and the persisted config. Defaults preserve
change-based publication. Existing configs receive default values in the config
response, without requiring a write first. Missing fields in partial updates
preserve their previous values. Keys and defaults:

| Key | Default | Meaning |
| --- | --- | --- |
| `dynamic` | `true` | Automatically publish qualifying changes |
| `periodic` | `false` | Legacy option to check for qualifying changes periodically instead of each sample |
| `interval` | `60` | Periodic check interval in whole seconds, 3–86400 |
| `minInterval` | `0` | Minimum time between event-mode publications, 0–86400 seconds |
| `maxInterval` | `0` | Publish even unchanged values after this many seconds; 0 disables, maximum 86400 |
| `temperature` | `0.5` | Absolute change in the configured temperature unit (°C or °F) |
| `humidity` | `1` | Absolute change in percentage points |
| `pressure` | `1` | Absolute change in hPa |
| `gas` | `5` | Absolute change in kΩ |
| `battery` | `1` | Absolute change in percentage points |
| `lux` | `5` | Absolute change in lx |

Thresholds accept finite JSON numbers from 0 to 1000000. A threshold of 0 accepts
any actual change, but does not send unchanged values. The comparison is inclusive,
with float-representation tolerance, against the last successful automatic MQTT
publication, so small changes accumulate. A change in availability/read-error state
also qualifies. Any qualifying field publishes the complete existing `sensor`/
legacy `dhtsensor` payload; lux is evaluated separately. If a required publication
fails, its baseline is preserved and retried on a later sample. Master/device topics,
retained messages and sensor payload keys remain unchanged. New broker connections
reset the two baselines for initial delivery. HA discovery seeds and explicit forced
publications bypass the normal filter; discovery seeds are independent of the
automatic publication baseline. Buttons, availability and direct API queries are
unaffected. Automatic brightness and WebSocket updates retain their sampling cadence.

A nonzero maximum must be at least the active check interval or event minimum
when dynamic sending is enabled. With both modes disabled, automatic sends stop.
The threshold defaults apply to fresh configurations and to missing thresholds
in saved configurations. Explicitly saved thresholds, including zero, are preserved.

The top-level `mqttMatrixInfoInterval` sets the automatic MQTT Matrixinfo cadence
in whole seconds (10–86400, default 15). WebSocket status keeps its existing
cadence. Immediate status updates triggered by device events and explicit
requests are still available.
Invalid types, ranges or interval combinations preserve the entire previous policy
object; other existing config fields still follow their normal handling. Validation
is logged, not a new configuration error protocol. Intervals are evaluated at the
existing sampling cadence (sensor 3 seconds, lux at its existing cadence), with
wrap-safe millis arithmetic. There are no added timers, sensor reads or JSON parses
in the loop. The settings and two snapshots use fixed storage. Snapshots are captured while the existing sensor JSON is assembled.
The persisted/returned JSON contains a bounded sensor policy (11 members) and the Matrixinfo interval. Configuration
serialization/parsing needs additional transient heap; no message/buffer limits or
pinned dependencies are raised. Physical ESP8266 minimum heap/largest-block and
frame-jitter validation under concurrent load remain outstanding for this change.

Host verification: `python3 test/sensors/run.py` exercises the actual policy, JSON
settings and notification functions, including accumulated changes, periodic gates,
forced/max sends, failed publishes, invalid settings, millis rollover and independent
WebSocket/lux delivery. Also run all six builds plus HA and animation host suites.

Companion inspection: PixelIt.WebUI Options gains bounded numeric inputs and shows an update hint instead of editable controls
on firmware without the object; its demo fixture includes defaults. Publish
the matching bundle and regenerate `include/Webinterface.h` through `.github/webui.py`
before firmware delivery. The workflow's legacy `pixelit-project/WebUI` path redirects
to PixelIt.WebUI (previously verified 2026-11-02); integration is unchanged. Node-RED
passes configuration objects through and requires no format/topic changes.
PixelIt.API serves bitmap/release/telemetry services and needs no change. External
PixelIt.Docs must document the policy, units, defaults and examples when the firmware
reaches stable; its stable-release baseline excludes unsolicited unreleased features.

Verification for this change on 2026-11-04: all six board builds and sensor-policy,
Home Assistant and animation/preview host tests passed. WebUI numeric-validation,
draft/usability and update tests, targeted ESLint and the production build passed
(the build retains webpack bundle-size warnings). Demo browser checks covered mode
switching, decimal thresholds, invalid intervals/negative thresholds and clearing
errors after correction. No device firmware/configuration was changed; physical
MQTT/sensor, reconnect, frame-timing and peak-memory checks remain outstanding.


## Matrix progress during HTTP updates (unreleased)

The existing `/update` multipart handler displays a white percentage, a fixed
upward arrow with moving violet/pink logo colors, and a straight two-pixel-high
violet progress bar with a pink light impulse. The bar spans x=11 through x=30
on the 32×8 matrix, leaving an inset at the right edge. Its light moves only
within the filled portion, so it remains visible before 100%. The arrow fades
in during the first 800 ms without delaying upload processing. ESP8266, ESP32 and Ulanzi use the
same design. Filesystem uploads also show progress. It temporarily replaces normal
content and uses brightness 16 if the display was off. The installed firmware must
already contain this feature: the first upload installing it still uses the old
handler. USB flashing cannot show progress through this firmware.

During transfer, the percentage uses received file bytes divided by the multipart
request length, which includes headers/boundaries and other form fields. It is an
approximation, capped at 99%; 100% appears only after `Update.end(true)` succeeds,
before the restart. After sending the successful HTTP response and closing the
upload client, the success hook animates 100% for 1.6 seconds and displays a
centered static `REBOOT` in a violet/pink logo gradient for two seconds, without
an icon or bar. Only then does the existing updater restart the device. This
completion phase adds 3.6 seconds of yielding holds plus LED transfer time, with no
application-handler reentrancy; normal playback is already suspended for OTA.
Failure/abort does not enter the success/reboot display phase. Unknown request
lengths remain at 0% until completion.
An authenticated upload failure or abort shows red `ERR`; normal rendering can
resume once the handler returns, so the error is not a persistent status screen.
The HTTP response remains the source of detailed update errors.

`extra_script.py` instruments build-local copies of the pinned HTTPUpdateServer
headers with guarded upload/reboot hooks, leaving installed frameworks,
authentication and responses intact. Framework upgrades must review this hook as well as the existing
HTTP/transport patches. ESP32 collects one additional small request header
(`Content-Length`); ESP8266 uses its existing upload length field. The renderer
uses the existing matrix buffer, shared 24-byte flash-resident arrow masks/palette
and small fixed state, without JSON or extra pixel allocations. It leaves normal
font/cursor settings untouched. LED transfers occur at most every 100 ms for changed
percentages or color/light phases, except start, completion and the first failure.
The arrow color phase advances every 200 ms, the bar light every 100 ms, while
upload callbacks run. There is no separate ticker during flash writes or stalled
reception. Background rendering service skips animation and scrolling frames while
an upload owns the display. Normal playback scheduling is otherwise unchanged;
OTA remains the existing synchronous flash-writing operation.

The notification is protected against ordinary incoming screen content for
15 seconds so the scrolling text can be read.

After a successful version check discovers a newer release, the enabled matrix
notification appears in the same loop iteration instead of waiting for the first
30-minute reminder. Further reminders are timed from the actual display; repeated
checks of the same version do not trigger another immediate notification. A newly
discovered release queues a notification while sleeping or while matrix update
notifications are disabled, and displays when enabled/awake. An equal/older release
clears the pending notification. Checks still begin after 30.5 seconds, repeat
every eight hours and defer during animation/scrolling. One fixed pending flag is
added, without new background requests, buffers or changes to playback scheduling.

The firmware-available notification uses the identical fixed arrow and eight color
phases at 200 ms beside the existing white `New FW available` scrolling text.
Text begins at x=8 to avoid overlapping the eight-pixel-wide arrow. Construction
now lives in `src/UpdateScreen.cpp` with checked JSON additions, using existing
internal compact pixel strings rather than a JSON node for each pixel. One bounded
194-byte stack buffer is reused and copied into the JSON buffer for each frame.
Eight compact frame strings replace seven nested arrays containing 448 pixel
nodes, reducing transient JSON storage. The committed 8×8 animation uses 1,024
pixel bytes rather than 896 (+128 bytes), within the existing platform limits.
No animation/message limits or dependencies were increased. Construction failure
skips `CreateFrames`, preserving the current content. Physical ESP8266 peak heap,
largest block and repeated-notification checks remain necessary.

Validation: `python3 test/update/display.py` covers the actual callback's progress,
rate limit, bar animation, finalization, logo-gradient REBOOT layout and bounded
success-only completion timing, failures, abort, unknown length, visibility
at zero brightness and 32-bit byte-count overflow.
`python3 test/update/notification.py` checks the shared silhouette and logo palette
at every phase, exact compact-pixel decoding, and full/partial JSON allocation
failure. `python3 test/update/scheduling.py` covers immediate discovery, repeated
checks, reminder boundaries, disabled/sleeping displays, current/older releases and
millis rollover. Run all six builds plus the existing update and animation/preview
tests.
Physical OTA checks on ESP8266, ESP32 and Ulanzi, failed/aborted uploads, concurrent
frame timing and ESP8266 peak heap/largest block still require designated-device
validation. The 192.168.0.86 design previews only replaced display content; no
firmware or configuration was changed. Builds and host tests do not establish
device timing or RAM safety. On 2026-11-04, all six firmware builds and the
progress, notification, release-comparison and animation/preview host tests passed
for the final accepted design.

Companion inspection: PixelIt.WebUI's Update view already sends the same multipart
`firmware`/`filesystem` uploads to `/update` and needs no client change. No payloads,
settings, topics or endpoint contracts changed. External PixelIt.Docs
`src/webinterface.md` should explain the on-device progress, notification design,
approximate percentage, errors and first-install limitation when this reaches
stable; its current stable baseline is unchanged.


### Designated-device OTA install — 2026-11-04

At the user's request, the final design build (`0.0.0-beta`, `ESP32_generic`)
was installed over HTTP OTA on 192.168.0.86, previously 2.7.0-beta.9. The updater
returned HTTP 200 and `Update Success! Rebooting...`. Read-only checks confirmed
the new build, software reset and advancing uptime (10 to 22 seconds). Existing
configuration fields were preserved except the normal stored version; the new
`mqttSensorPublish` object appeared with compatibility-preserving defaults.
The temporary configuration comparison files were removed afterwards.

This installation ran the previous firmware's updater, so it does not validate
the new progress renderer during an actual OTA transfer. The accepted progress
and notification designs were previewed as screen content before installation.
Failed/aborted OTA, new-renderer timing and ESP8266 peak-memory checks remain
outstanding; this install does not replace those validation requirements.


### Immediate update notification correction — 2026-11-04

The notification now queues immediately after discovering a newer release and
repeats 30 minutes after display. The existing five-second protection against
ordinary incoming screen content remains unchanged. All six builds plus update
scheduling, release-comparison, progress, notification, animation/preview and
Home Assistant host tests passed.

The correction was installed on the previously designated ESP32_generic at
192.168.0.86. This OTA client's connection was reset before an HTTP success
response, so no successful response was recorded. Subsequent read-only checks
found the device reachable with a software restart and completed version check;
the user confirmed that the notification appeared immediately. The configuration
comparison found no changes and its temporary backup was removed. Both builds
use `0.0.0-beta`; that version string alone cannot distinguish them. The user's
behavioral observation supports the scheduling correction, without establishing
all-board timing or ESP8266 peak-memory safety. No ESP8266 device was flashed.


### Fifteen-second notification protection — 2026-11-04

At the user's request, `CHECKUPDATESCREEN_DURATION` was increased from five to
15 seconds. No buffers, allocation limits or background work changed. All six
builds and update scheduling/version plus animation/preview host tests passed.
The new ESP32_generic build was installed on 192.168.0.86 with an HTTP 200
`Update Success! Rebooting...` response. Read-only checks confirmed a software
restart and unchanged settings. At uptime 38 seconds, the version check was
complete and the device still reported `New FW available` with eight animation
frames, consistent with the longer protection beyond the previous five seconds.
This was not a full physical timing/heap test or a measured 15-second duration
trace. The temporary settings comparison file was removed.


### Accepted flash-screen polish — 2026-11-04

The accepted design keeps the fixed logo-color arrow, insets the straight bar,
and moves a restrained pink light only through its filled portion at all progress
levels. Successful completion retains the animation at 100%, then replaces the
whole screen with centered static logo-gradient `REBOOT`, without icon or bar.
The yielding success-only holds total 3.6 seconds plus rendering time and occur
after sending the response/closing the client, immediately before restart. No
pixel buffer or JSON allocation was added to the OTA display path.

All six final builds and progress/completion, shared-arrow notification,
notification scheduling, release-comparison, animation/preview and Home Assistant
host tests passed. Completion tests check the 3.6-second hold budget, REBOOT
placement/gradient and exclusion of success holds on failures. The final
ESP32_generic build was installed on 192.168.0.86 with HTTP 200 and the updater's
success/reboot response. Read-only checks found software reset, uptime 29 seconds
and unchanged settings; the temporary comparison file was removed. This transfer
ran the previous build's updater: the new completion path still needs a subsequent
physical OTA validation. ESP8266 timing, peak heap/largest block and failure/abort
device validation remain outstanding. No ESP8266 was flashed.


## Boot and WiFi visual alignment (unreleased)

Boot lettering uses the same violet-to-pink gradient as `REBOOT` for `PIXEL`,
with the final `IT` in white, retaining
the existing letter sequence, font, positions and 2.3-second delay budget.
`LogoGradientColor` in `src/UpdateVisuals.cpp` supplies the identical RGB565
colors to both screens. Boot stages recolor only lit pixels through the existing
logical-to-physical `matrix->XY` mapping; no extra pixel storage is used.

The WiFi screen keeps its white label and existing one-pixel bouncing trail,
60 ms ticker, trail lengths and brightness falloff. Its trail uses the logo
violet/pink gradient instead of blue, including the initial frame. PixelItFont
stores eight-column glyphs with blank padding, so nominal GFX bounds are not
visible-ink bounds. The startup renderer now measures the actual illuminated
`WiFi` pixels once in the existing matrix buffer, centers them horizontally and
vertically above the reserved trail row, and reuses that placement for each tick.
The cached coordinates/flag add five bytes before target alignment; no new heap
allocations, timers or frame buffers are added. This occurs during connection
setup, before normal playback; runtime rendering/ticker cadence is unchanged.

`python3 test/startup/run.py` renders the actual startup functions with the real
PixelItFont bitmap on a host matrix, checking boot gradient/delays, centered
visible ink, cached placement, both trail directions, initial trail color and
serpentine matrix mapping. Run all six board builds, update display/notification
and animation/preview host tests. Physical boot/WiFi appearance and ticker timing
still require device observation; host tests do not establish ESP8266 peak-memory
or playback timing safety. No endpoint, setting or payload contract changes are
required. External PixelIt.Docs startup screenshots/descriptions should reflect
the logo palette and WiFi layout when this reaches stable; companion client code
requires no changes.


### Startup palette and WiFi install — 2026-11-04

All six builds plus real-font startup/centering, update display/completion,
shared-arrow notification and animation/preview host tests passed. The initial
ESP32_generic OTA attempt to 192.168.0.86 ended with curl `Broken pipe` and no
success response; subsequent status showed advancing uptime without a restart.
A second upload limited to 64 KiB/s returned HTTP 200 and the updater's success/
reboot response. Afterwards the device reported software reset and uptime 21
seconds, with unchanged settings. The temporary comparison file was removed.
Boot display was already enabled and battery startup display disabled; neither
setting was changed. These checks confirm deployment and host-rendered placement,
not an independent physical observation of startup colors/centering. ESP8266
physical timing/heap and startup tests remain outstanding; no ESP8266 was flashed.


### White IT boot variant — 2026-11-04

The user accepted the real-font screen preview with `PIXEL` in the logo gradient
and the final `IT` in white. Boot recoloring now preserves white pixels from the
I/T columns; REBOOT and the centered WiFi screen retain their accepted colors.
All six builds and startup palette/centering, OTA display/completion and
animation/preview host tests passed.

The matching final builds were installed sequentially on the user-designated
192.168.0.40 (ESP8266_d1_mini) and 192.168.0.86 (ESP32_generic), both with HTTP 200
and the updater's success/reboot response. Read-only checks confirmed software
restarts, uptimes 38/17 seconds respectively, MQTT connected and unchanged settings.
Temporary configuration comparison files were removed. These deployment checks
do not establish physical waveform timing, long-duration stability or ESP8266
peak heap/largest-block safety under concurrent workloads.


## Boot animation selection (unreleased)

In the matching WebUI, Options → Defaults → **Boot animation** selects
**Gradient letters** (`logo`, the default) or **Multicolor letters** (`classic`,
the original per-letter colors). The firmware persists `bootScreenStyle` and applies it at
startup when `bootScreenAktiv` is enabled. The WebUI hides the selector if the
firmware does not advertise a supported value. Existing clients can continue
to omit the setting. At stable release, document the selector in
PixelIt.Docs's `src/webinterface.md` and the config field in `src/api.md`.

## Optional entire WiFi screen (unreleased)

`wifiScreenAktiv` is an optional boolean configuration setting, independent of
`bootScreenAktiv`, defaulting to `true`. Set it to `false` to suppress the entire
startup WiFi connection display (text and moving trail) as well as the matrix
HOTSPOT notice. The connection/configuration portal still works and keeps its
existing logs. With the WiFi screen disabled, the last startup image (boot logo
or battery screen, if enabled) stays visible until the next normal display
update. The WiFi animation ticker is skipped, and the tick and hotspot callbacks
leave the display alone. The setting takes effect at the next startup.

The setting follows existing HTTP/MQTT/WebSocket config writes, persisted config,
and config reads. Missing keys preserve the current value; legacy saved files
use the enabled default. Only JSON booleans are accepted; null, numbers, strings
and objects preserve the previous choice. Config reads advertise the validated
current boolean even if the file predates it, with a checked JSON allocation.
One boolean plus one small JSON member is added, without new timers, pixel
buffers, message limits or periodic parsing. Repeated config serialization still
needs physical ESP8266 peak-heap/largest-block validation; builds do not prove
RAM safety. Ordinary playback paths and connection timing remain unchanged.

Companion PixelIt.WebUI adds **Show WiFi screen during startup** beside Bootscreen
active. On old firmware without the boolean it shows an update hint instead of an
editable control; no missing field is silently interpreted as off. Demo data has
the enabled default. WebUI code changes live in that repository, not the generated
firmware header. Node-RED's configuration forwarding preserves the boolean and
requires no client changes; PixelIt.API is not involved in device settings.
External PixelIt.Docs needs the option, enabled default, startup scope, retained
startup image and hotspot behavior in its WebUI/firmware guide at stable release.

The WebUI production build references externally hosted hashed assets. Its matching
bundle must be published by the normal WebUI release pipeline before regenerating
`include/Webinterface.h` with `.github/webui.py`; pointing installed devices at
unpublished hashes would break the WebUI. A clean release bundle (without the local development device IP) and its embedded
index have been generated with `.github/webui.py`. Those final binaries must not
be installed until the new asset hashes are published. Existing devices have not
been updated for this option yet. No public deployment was performed for this
development change.

Validation commands: `python3 test/config/wifi_screen.py`,
`python3 test/startup/run.py`, `python3 test/animation/run.py` and all six builds.
The config test exercises actual setter/save fragments with old/missing keys,
false round trips and invalid input; startup tests exercise the disabled tick and
hotspot branches, enabled trail and actual-font alignment. WebUI targeted lint,
production build and usability, sensor-publish and update tests also pass. Demo
browser inspection verified the new switch toggles independently of Bootscreen
active, marks its isolated draft dirty, and returns to enabled after discarding.
No browser test sent a device config write. Physical disabled-startup observations,
ESP8266 timing and peak-memory checks remain outstanding.


## Retained ESP32 network diagnostics (unreleased)

`GET /api/animationinfo` adds an optional `network` object on ESP32. Existing
fields and MQTT/WebSocket payloads are unchanged. Counters reset on reboot,
not when a new animation starts. Read it after an outage alongside matrix-info
uptime, RSSI, BSSID and MQTT connection state. This is diagnostic instrumentation,
not a correction of the reported disconnects.

- `wifiStatus` is the Arduino WiFi status at the diagnostic request.
- `wifiDisconnects` counts station-disconnected events; `wifiGotIP` counts IP
  acquisition events, including initial startup. A got-IP event alone is not
  proof that HTTP/MQTT is reachable.
- `wifiLastReason` is the numeric disconnect reason from the pinned ESP32 SDK
  (zero before any disconnect); interpret it against that SDK's `wifi_err_reason_t`,
  not HTTP status codes. `wifiLastDisconnectMillis` and `wifiLastGotIPMillis`
  are the last respective event times in milliseconds since boot.
- `mqttSlowLoops` and `mqttSlowConnects` count `client.loop()` and the complete
  `MQTTreconnect()` wrapper respectively when duration is at least 250 ms.
  Connect timing includes subscription/initial publications/discovery inside the
  wrapper; loop timing excludes subsequent `ServiceHADiscovery()` work.
- `mqttMaxLoopMillis` and `mqttMaxConnectMillis` retain maximum durations.
  `mqttLastSlowMillis` is the completion time of the latest slow call;
  `mqttLastSlowDuration` is its elapsed milliseconds, `mqttLastSlowWasConnect`
  identifies the wrapper, and `mqttLastSlowState` is PubSubClient's state after
  returning. State zero can accompany a slow successful call. These fields start
  at zero/false; check slow counters before interpreting them. All millisecond
  values wrap at 32 bits. WiFi and MQTT observations need not have the same cause.

For example, an increased `wifiDisconnects` with a new reason/timestamp followed
by `wifiGotIP` supports a WiFi link interruption. Unchanged WiFi counters with
increased slow MQTT counters points to application-side waiting for further
investigation, without excluding TCP or access-point trouble. Historical
WebSocket send timeouts alone cannot identify which peer or network caused it.
No new events during an outage also does not prove the network is healthy.

WiFi event-task callbacks only update fixed storage inside a short critical
section. They never allocate, log or call network/rendering code. MQTT timings
use the same bounded state; JSON allocation happens only on explicit requests.
No buffers, limits, dependencies, reconnect policy or playback waits changed.
ESP8266 retains its existing diagnostic response and adds no diagnostic storage.
New ESP32 state is one small fixed struct plus a critical-section lock; physical
frame-jitter and concurrent network validation remain required after deployment.

Companion inspection: local PixelIt.WebUI, node-red-contrib-pixelit, PixelIt.API
and PixelIt.Docs sources have no consumers of `animationinfo`, `webSocketSend`
or `slowestTask` requiring adjustment. No client change is needed for the additive
object. External PixelIt.Docs should document these fields, units, reason-code
interpretation and examples at stable release; its stable baseline is unchanged.
Run `python3 test/esp32/network_diagnostics.py`, the animation/preview and ESP32
socket-output host suites, and all six builds. The new test exercises actual WiFi
callbacks, retained event history, timing thresholds, separation of connect/loop
measurements and 32-bit timer rollover. It does not simulate radio disconnections.

Validation on 2026-11-04: all six firmware variants built successfully; retained
network diagnostics, animation/preview, ESP32 socket-output/static-preview and
Home Assistant host tests passed. No diagnostic firmware was installed and no
settings or display content were changed during this implementation. Physical
WiFi interruption/recovery, frame jitter and concurrent peak-memory validation
remain outstanding. Existing unrelated startup-style work in the working tree
was preserved.


### Designated ESP32 diagnostic install — 2026-11-04

At the user's request, a diagnostic-only staging build (`0.0.0-beta`,
`ESP32_generic`) was installed on 192.168.0.86, previously 2.7.0-beta.10.
The current repository WebUI index references unpublished assets (HTTP 404), so
this staging build used the device's existing index, generated through
`.github/webui.py`; all four referenced JS/CSS assets returned HTTP 200.
Pre-existing uncommitted `bootScreenStyle` changes were excluded in the staging
copy; the working tree was preserved. The separate ESP32 build passed.

The first OTA attempt at 64 KiB/s ended in a connection reset without a success
response; repeated status checks still showed the old version, advancing uptime
and no diagnostic fields. A second attempt at 32 KiB/s returned HTTP 200 and
`Update Success! Rebooting...`. Read-only checks then confirmed software reset,
uptime nine seconds, MQTT connected and the new `network` fields. The configuration
comparison found only the normal stored `version` change. The deployment binary
SHA-256 is `6eeeb414a2738f5d5095175a01ba3d9d04e7209f2b296e3db17346fa13907f6e`;
its binary, manifest and non-sensitive verification result are retained under
`.pio/build/ESP32_generic/firmware_network_diagnostics_deployment*` and
`network_diagnostics_deployment_verification.json`. Temporary settings backups
and the staging directory were removed after verification.

A short read-only post-install check received 86 complete WebSocket JSON messages,
including 78 previews, without parse/connection errors. Four HTTP samples at uptime
33–40 seconds took 40–63 ms and all reported MQTT connected. The device-served
WebUI index was byte-identical to its pre-install index. At the final diagnostic
sample there were no recorded WiFi disconnects, slow MQTT calls or WebSocket send
failures; the maximum animation interval was 211 ms at 200 ms configured, with
zero failed LED frames. These are short deployment observations, not evidence
that the intermittent outage is corrected or a physical jitter/peak-memory
validation. No test screen payload or configuration write was sent. The new
counters are ready to inspect after a recurrence and reset on any reboot.


## ESP32 failure cleanup after a stalled WebSocket (unreleased)

The 2026-11-05 observer (00:00–00:03 Europe/Berlin) captured a disturbance after
16 WebSocket animation commands: combined matrix/animation HTTP reads reached
8,008 ms, output timeout counts rose from one to five, and the recorded WebSocket
task maximum was 1,781 ms with a 1,964 ms animation interval at 200 ms configured.
The observer itself received 1,295 JSON messages without a reconnect; other client
slots disconnected/reconnected. No WiFi-disconnect event or restart was recorded;
MQTT was connected at every sample, with one 377 ms loop call. Sampled free heap
fell as low as 59,900 bytes and later recovered above 150 KB. These samples do not
identify allocation-time minima or establish the original cause of backpressure.

Code inspection found that timeout/overflow cleanup reset the queue's `enabled`
flag while the WebSocket library could still consider its client connected. A
subsequent send (including sendFrame's payload after its header failed) could then
enter the synchronous handshake writer with a five-second retry deadline. That
writer services display frames but excludes other application network handlers.
The host regression reproduces the invalid lifecycle on the previous source;
causality for every device outage remains to be established.

Failed queues now retain nonblocking routing and a fixed failed flag until the
outer flush loop explicitly disconnects the native WebSocket client. Further
writes fail immediately without reallocating/reopening the queue. Retirement runs
outside sendFrame and its enqueue-time flush to avoid reentrant close callbacks.
Reconnect callbacks reset the failed state, including reused file descriptors.
Failed viewers are excluded from preview readiness. The existing handshake writer,
five-second no-progress policy, ring sizes and dependency versions are unchanged.
One bool per ESP32 viewer is added; no ESP8266 storage/path change is introduced.

The socket-output host suite covers timeout retirement, overflow followed by another
payload write, rejection until cleanup and same-fd reconnects. The actual patched
writer test verifies that failed queued writes return without entering the timed
writer. Also run ESP32 preview/receiver, animation, Home Assistant and all six
builds. No payload, endpoint, settings or companion client changes are required.
The existing companion-source inspection applies; external PixelIt.Docs should
include this failure/recovery behavior in the stable-release troubleshooting guide.
Physical stalled-client isolation and HTTP/MQTT responsiveness must be checked;
ESP8266 physical timing/peak-memory coverage remains unchanged.


### Failure-cleanup deployment and remaining latency — 2026-11-05

All six final board builds and animation/preview, ESP32 receiver/writer,
socket-output, static-preview and Home Assistant host tests passed. Running the
new lifecycle regression against the previous SocketOutput.cpp failed as expected.
The isolated ESP32_generic deployment retained the published device WebUI and
excluded unrelated startup-style edits, as in the preceding diagnostic deployment.
The 32 KiB/s OTA returned HTTP 200 with `Update Success! Rebooting...`; subsequent
read-only checks confirmed software reset, uptime ten seconds, MQTT connected,
byte-identical WebUI index and no configuration changes. The binary/manifest are
`.pio/build/ESP32_generic/firmware_socket_failure_fix_deployment.*`.

A first 30-second deliberately unread viewer test triggered an expected timeout;
HTTP pairs took at most 189 ms and MQTT stayed connected. Its reading observer
incorrectly used a six-second idle timeout and exited during the initially static
screen, before the update-notification animation began; it does not validate
reading-viewer survival. That result is retained as the idle-reader attempt.

With the observer timeout corrected, a second 35-second run received 132 complete
WebSocket messages without reader errors while the deliberately unread viewer was
rejected (503/ENOBUFS; send failure count four to five). MQTT was connected at all
samples, and there was no reboot or WiFi-disconnect event. However, combined HTTP
reads reached 5,555 ms; the recorded HTTP main-loop maximum reached 1,505 ms and
the animation maximum interval 1,518 ms at 200 ms configured. A 916 ms MQTT-loop
outlier was also present. Thus the cleanup regression is corrected, but the
reported overall latency is not resolved. Further investigation must distinguish
HTTP send/parse waits, remaining handshake waits, TCP/WiFi congestion and reconnect
bursts, rather than treating this short client-survival test as a timing pass.
No test screen or configuration payload was sent. Settings backups and staging
sources were removed after verification. Non-sensitive results remain in
`.pio/socket_failure_fix_device_validation.json` and `.pio/network-watch.jsonl`;
the scheduled monitor continues. ESP8266 physical timing/heap coverage is unchanged.


## Staggered ESP32 WebSocket initial state (unreleased)

The failure-cleanup fix alone did not resolve the user's nearly unusable WebUI.
After the observer test ended, HTTP-pair samples still took 11,117 / 7,041 / 2,289 ms
(the per-read socket timeout is not a total-request deadline). Heap recovered but
latency remained. Reconnect/startup output also recorded 503/ENOBUFS, motivating
removal of the synchronous six-message startup burst rather than increasing RAM.
This is a candidate congestion/reconnect correction; device results follow separately.

ESP32 now marks a newly connected viewer as awaiting initial state. The main loop
builds and queues at most one initial message total per iteration, only when that
viewer's previous output is empty and the viewer remains connected/non-failed.
The six unchanged messages retain order: sysinfo, lux sensor, environment sensor,
config, buttons, telemetry. Until completion, previews are skipped and ordinary
snapshot broadcasts are deferred (their initial counterparts supply current state).
Explicit command replies and logs retain their existing behavior. Disconnect
cancels the pending sequence; same-fd reconnection starts fresh. The final preview
catch-up still supplies static images once initialization finishes.

One uint8 step per ESP32 viewer is added to the existing queue objects, without
new queues, retained JSON or buffer/limit/dependency changes. An empty queue's first
fd assignment preserves its pending step. ESP8266 retains its immediate startup
messages and storage. Single initial messages larger than the existing ring can
still be rejected; this change does not increase supported configuration sizes.
Building one snapshot can still incur existing sensor/JSON work; real timing and
peak-memory checks remain necessary.

`python3 test/esp32/initial_state.py` exercises the actual scheduler for six-message
ordering, one-message work limits, busy/failed/offline viewers, retry and disconnect
cleanup. The queue regression checks preview gating and preserving the initial step
on first fd assignment. Run animation/preview, ESP32 transport/output, Home Assistant
host tests and all six builds. Existing companion clients accept the same independent
JSON messages; PixelIt.WebUI source inspection found no combined-message requirement.
No client changes are needed. External PixelIt.Docs should explain incremental UI
initialization/reconnect troubleshooting for the stable release.


### Staggered-initialization deployment — 2026-11-05

Before deployment the user also observed stalled physical animation. One read-only
snapshot recorded a 3,846 ms frame interval and 3,750 ms WebSocket task maximum,
17 send failures, no WiFi-disconnect event, MQTT connected and 70,236 bytes free
heap. Process inspection confirmed no remaining observer/load-test processes.
Those observations belong to the preceding cleanup-only build, not this candidate.

All six final builds and staged-initial-state, socket-output, preview catch-up,
ESP32 transport/writer, animation/preview and Home Assistant host tests passed.
An isolated staging build again retained the device's published WebUI and excluded
uncommitted startup-style changes. OTA at 32 KiB/s returned HTTP 200 with success/
reboot response. Post-install read-only checks confirmed software reset, uptime
19 seconds, unchanged config and a byte-identical WebUI index. Binary/manifest:
`.pio/build/ESP32_generic/firmware_socket_initial_state_deployment.*`.

One normal reader received all six initial messages in 131 ms and was then closed;
no new stalled-client or animation-injection test was performed. Initial HTTP-pair
samples took 71–87 ms, but no animation was active yet, so those are not playback
measurements. With the normal firmware notification active at uptime 44 and 59
seconds, rendered-frame counts advanced from 63 to 138 at 200 ms configured,
maximum interval 201 then 205 ms, no failed LED frames, zero WebSocket send
failures and no WiFi disconnects/slow MQTT calls. HTTP pairs took 103/118 ms,
MQTT stayed connected and sampled free heap was 151,152/144,464 bytes.
These short low-load deployment samples are encouraging, not a reproduction of
repeated user commands/reconnect bursts or proof of a complete fix. Long-duration
multi-viewer/UI-command timing, peak memory and the user's physical observation
remain outstanding. The existing automatic monitor continues without an active
streaming observer or deliberate stalled client. Settings backups/staging sources
were removed after verification; non-sensitive results are retained in
`.pio/socket_initial_state_deployment_verification.json`.


### Follow-up heartbeat: IP-level loss — 2026-11-05, approximately 00:20 CET

At uptime 121 seconds on the staggered-init candidate, two new WebSocket send
timeouts were present. Matrix/animation HTTP reads took 2,247/2,481 ms, while the
maximum frame interval remained 205 ms at 200 ms configured and the slowest
recorded application task was only 42 ms. MQTT was connected; no WiFi-disconnect
event or reboot was recorded. Heap was 73,616 bytes with a 47,092-byte largest
block and a 58,936-byte boot low-water mark. One normal reader then completed the
handshake in 978 ms and all six initial messages in 1,229 ms, without parse errors.

A four-packet ICMP probe to the ESP32 received only one reply (784.656 ms; three
missing within the short test window). Immediate four-packet controls received
all replies: gateway 192.168.0.1 averaged 0.372 ms, designated ESP8266 192.168.0.40
averaged 4.940 ms. These brief, sequential comparisons show degraded IP-level
reachability to the ESP32 in addition to HTTP/WebSocket symptoms. They do not
establish RF interference, AP behavior, ESP32 driver/congestion or another root
cause, nor equal AP association for the control device. Good RSSI and absence of
a disconnect event do not exclude IP packet loss. The overall issue remains open;
application frame timing was healthy during this particular delayed-network sample.
No settings, content or firmware were changed during these heartbeat checks.


## Shared ESP32 preview budget (superseded diagnostic candidate)

A temporary isolated ESP32 A/B build paused only preview transmission at uptime
60–120 seconds, retaining rendering, commands and two existing browser clients.
The device settings and published WebUI were preserved. The test changed no saved
configuration or display content; the ordinary update notification was animating.
Host checks verified pause boundaries and latest/static-image catch-up on resume.
This diagnostic build must not be shipped: its timed pause is absent from source.

Before the pause, uptime 40/50-second samples had 10.408/3.585 ms ping and HTTP reads
28–58 ms. During the pause, heap stabilized around 157 KB; most HTTP reads were
31–309 ms, with one 1,704 ms response at the boundary and one ping without a reply
in the short probe window. At resume (uptime 120), ping was 889.970 ms, HTTP reads
2,041/1,486 ms and free heap 108,176 bytes, about 49 KB below the preceding paused
sample, with the same two clients. Uptime 130/140/150 pings were 893/573/488 ms;
HTTP remained 611–1,618 ms and heap 100–121 KB. Maximum frame interval remained
217 ms, with zero WebSocket send failures. These observations associate preview
traffic with network queuing; they do not establish the RF/AP/driver-level cause.

The host browser's two WebSocket TCP connections had zero receive/send queues at
inspection. A normal reading HTTP probe measured 342 ms connect, 600 ms first byte
and no host TCP retransmitted packets in that sample. Packet capture was unavailable
without administrator authentication, so no wire-level capture was obtained.
Non-sensitive A/B samples are in `.pio/preview_ab_measurements.json`.

ESP32 now admits at most one complete preview every 250 ms across all viewers,
about 6.2 KB/s of JSON payload total for the unchanged 32×8 preview, plus transport
headers and other messages. Ready viewers share turns in round-robin order;
blocked/disconnected/initializing viewers do not consume turns. Unsent samples
are replaced by the latest shared buffer, including static-final-image catch-up.
One viewer sees up to four preview updates/s; two share approximately two each,
five approximately 0.8 each. This deliberately reduces preview sampling rather
than device animation cadence. Commands, status, logs and initial-state messages
retain priority and bypass the preview pacing. LED frame scheduling is unchanged.

The budget uses one fixed timestamp, cursor and sent flag in the existing server
object, without new per-frame allocations or buffers. ESP8266 code, message/animation
limits, dependency versions and MQTT policy are unchanged. Millis rollover, aggregate
rate, five-viewer fairness, latest-image delivery, busy/disconnected clients and
uncapped command replies are covered in `test/esp32/preview_output.py`. Run all six
builds, animation/preview, ESP32 transport/queue/initial-state and HA host suites.
No companion payload/client change is required; existing companion inspections
apply. External PixelIt.Docs should describe shared sampling and LED independence
in the preview/troubleshooting guide at stable release. Physical multi-viewer
latency and long-duration peak-memory/timing still require validation.


### Preview-budget deployment and device comparison — 2026-11-05

All six board builds and aggregate-preview/fairness/rollover, initial-state,
queue, ESP32 transport/writer, animation/preview and HA host tests passed.
The final isolated budget build removed both temporary A/B markers/pause code,
retained the published device WebUI, excluded unrelated startup-style work and
was installed at 32 KiB/s with HTTP 200. Read-only verification confirmed a new
uptime, absence of the temporary pause field, byte-identical WebUI and unchanged
settings both immediately after installation and after the longer check. The
binary/manifest is `.pio/build/ESP32_generic/firmware_preview_budget_deployment.*`.
Private settings backups and staging sources were removed after verification.

Eighteen sparse samples at uptime 40–210 seconds covered one to four established
local TCP viewers, including one normal reading observer for 150.1 seconds. Its
handshake took 388 ms; it received 190 complete messages including 167 previews,
without parse/connection errors, and then closed. No stalled-client test, display
payload or config write was made. Across the samples HTTP response times were
31–224 ms (median 41.5 ms); ping median was 5.348 ms, maximum 320.504 ms during an
additional connection. Later multi-viewer samples were mostly single-digit-ms
ping and sub-100-ms HTTP. MQTT stayed connected with no sampled restart, WiFi
disconnect or WebSocket send failure. Maximum frame interval was 227 ms at 200 ms
configured. Minimum sampled free heap was 128,924 bytes and largest block 55,284
bytes. These are sparse samples, not allocator-level minima or physical waveform
measurements. Results are in `.pio/preview_budget_device_validation.json`,
`preview_budget_normal_viewer.json` and `preview_budget_summary.json`.
The longer run supports the candidate under these ordinary viewer conditions;
full command/upload workloads, extended runtime, RF/AP causality, physical jitter
and ESP8266 peak-memory/timing coverage remain outstanding. The automatic monitor
continues without an active additional observer.

### Relationship to the branded update displays

The user asked whether the new flash animation could explain why ESP8266 works
while ESP32 stalls. Source comparison against `d9dc8fb^` shows the upload progress/
reboot renderer is entered only by upload callbacks and the pre-restart hook.
Its active flag initializes false at boot; it is not a normal-loop animation task.
The same commit also changed the firmware-available notification from 400 to
200 ms per bitmap frame and schedules it immediately after discovery. With
Liveview's half-frame sampling, that changes potential preview sampling from
five to ten images/s per viewer while the notification/scroll is moving. Before
that commit the notification first appeared at the 30-minute reminder, rather
than immediately after the version check. Development version `0.0.0-beta` sees
the published beta as newer, making the notification relevant after each install.

Thus the visual-update change can indirectly expose network pressure through
the new notification, even though the actual flash-progress renderer is inactive.
This is a code-supported traffic mechanism, not an isolated old/new-notification
A/B causality test. ESP8266 preview fragments wait for available TCP space and
hold the shared pending sample; ESP32 uses per-viewer queues and previously
skipped full viewers while admitting full-rate streams to ready viewers. Raw
CPU capability does not establish equivalent network scheduling/backpressure.
The shared ESP32 pacing addresses the measured traffic sensitivity while keeping
the accepted LED design and animation rate intact; it is not proof of the
underlying radio/AP/driver limitation being resolved.


## ACK-driven ESP32 preview backpressure (unreleased)

The user rejected the shared four-preview/s candidate because divided preview
sampling visibly stuttered. That cap is removed; the previous budget measurements
are historical results, not validation of this replacement.

A viewer can now receive another preview only after its preceding TCP output is
ACK-drained, with no unsent TCP data and room for one complete preview. Readiness
is sampled asynchronously on the lwIP core at most every 20 ms through one reusable
callback message. The main loop never waits for the callback. Busy viewers retain
only the latest shared sample; ready viewers take round-robin turns without a
fixed FPS ceiling. Command/status writes bypass preview admission and invalidate
cached readiness, as do bytes flushed to native TCP, disconnects and fd reuse.
The 20 ms probe cadence is not the LED or preview sampling interval.

The pinned ESP32 SDK's per-slot socket mutex is taken with zero wait around the
conn/PCB read, on the lwIP core, preventing close/free races without blocking the
core. The inspected lwIP source for ESP-IDF 4.4.6 is commit
`4f24c9baf9101634b7c690802f424b197b3bb685`; its slot mutex is allocated once and
never freed. The debug socket accessor's declaration needs C linkage. These are
private pinned-SDK interfaces: framework upgrades must review socket lifetime,
field layout, callback reuse and locking, not merely pass the current compile
checks. Build guards require the pinned socket-lock/full-duplex configuration.
No socket/PCB pointers survive the callback.

A fixed snapshot/result struct, connection/write epochs and ready flags are added
only on ESP32. The callback message is allocated once and reused; allocation
failure retries at most once per second, including millis zero/rollover. Queue
submission is nonblocking, with only one outstanding probe. Failures keep previews
pending while ordinary status/command services continue. No per-frame allocation,
pixel buffer, payload-limit increase or dependency upgrade is introduced. ESP8266
retains its existing fragment/available-space path. Wire JSON and the published
WebUI remain compatible; companion code needs no changes. External PixelIt.Docs
should describe sampled previews/backpressure and LED independence at stable
release, superseding the temporary shared-FPS-cap description.

`test/esp32/transport_probe.py` executes actual probe logic with delayed callbacks,
core-only PCB-read assertions, unacked/unsent data, busy zero-wait locks, insufficient
send space, stale write/fd results, allocation/submission failures and timer rollover.
`preview_output.py` checks absence of a fixed FPS cap for ready viewers, isolated
stalled clients, latest/static catch-up and uncapped replies. Run queue/initial-state,
ESP32 transport, animation/preview, HA tests and all six builds. Physical frame,
network, memory and actual received-preview cadence remain required before judging
this candidate; host tests alone do not demonstrate smooth visual playback.


## Visibility-aware preview subscriptions (unreleased)

The first ACK-driven four-viewer run was not a smooth-preview pass: one reader
received 798 images over its active window, averaging 5.07 FPS, median interval
108 ms but p95 621.3 ms and a 7,790 ms maximum gap. Later HTTP reads reached about
one second despite healthy ~220 ms LED frame intervals. The user-facing requirement
is therefore still open; removing the artificial cap alone was insufficient.
Raw results: `.pio/ack_preview_device_validation.json`.

ESP32 matrixinfo now advertises optional boolean `previewControl: true`. A WebSocket
client can send exactly `{ "preview": { "enabled": false } }` or `true` to pause or
resume only its own previews. The top-level object and nested object must each
contain only that member; enabled must be a JSON boolean. It is transient, never
saved, and resets to enabled on reconnect for legacy clients. Controls, status and
physical LED output continue. Resume marks the latest static/shared image for
catch-up. ESP8266 does not advertise the capability and adds no flag/storage.
`animationinfo.previewSubscribers` counts enabled non-failed ESP32 output slots
with an assigned fd; it does not count delivered images or TCP ACKs.

Companion PixelIt.WebUI supplies an inline, event-driven visibility controller
before the existing socket plugin. It sends subscription changes only after the
capability is advertised, pauses hidden pages, resumes visible pages and cleans up
closed sockets. The newest capable same-device socket within one document owns
preview subscription, avoiding duplicate streams during overlapping reconnections;
other devices are independent. There is no new timer, polling or dependency.
Existing open pages must reload once to load the controller; a socket reconnect
alone cannot update page code. Old firmware and old clients preserve their existing
behavior. Node-RED/API clients need no changes for the additive capability/command.
External PixelIt.Docs should document the optional per-viewer command, boolean
validation, transient/reconnect semantics, visibility behavior and diagnostic count
at stable release.

The WebUI production template inlines its source controller; a tested companion
`scripts/preview-compatible-index.cjs` also generates an isolated index retaining
already-published JS/CSS asset URLs, without publishing unreleased bundles. The
firmware header is still generated through `.github/webui.py`, not edited manually.
The root HTML handler now uses `send_P` to stream its PROGMEM index rather than
copy the larger inline index into a request-sized String, including on ESP8266.
No pixel buffers, limits or dependencies are raised. Extended response timing and
ESP8266 peak heap/largest-block checks remain necessary for that larger index.

Host checks: `test/esp32/preview_subscription.py` executes the actual parser with
invalid types, extra fields, viewer isolation and reconnect defaults; queue tests
cover subscription gating. Companion `tests/preview-visibility.test.cjs` covers
capability fallback, hidden/visible/page lifecycle, duplicate sockets, reconnection,
constructor/constants and idempotent published-index generation. Run the relevant
existing host suites, all six builds, companion targeted lint/production build and
browser visibility/reconnect flows. Device results must distinguish an actual
smooth preview cadence from merely surviving connections.

### ESP32 MQTT output stalls

The ESP32 diagnostic run with a 400 ms update notification recorded a 15,372 ms
matrix frame gap and a 10,011 ms MQTT section. Separate diagnostics recorded a
4,492 ms PubSubClient loop and a 3,843 ms reconnect. No WLAN disconnect or
WebSocket write failure was recorded. No preview subscriber was active when
sampled after the stall.

The pinned ESP32 WiFiClient can wait ten times for one second in a write.
PixelIt limits MQTT writes to a 40 ms retry window with nonblocking socket
sends and services due display frames while waiting. An incomplete write
closes the connection to preserve MQTT framing. The MQTT reconnect interval
still applies. PubSubClient also services display frames while waiting for
CONNACK. TCP connection establishment, DNS and the cause of transient network
latency are separate from the write retry window. ESP8266 keeps its existing
transport.

### Compact ESP32 Liveview frames

ESP32 advertises `sysinfo.previewCompact`. A WebUI that supports this format
can send `{"preview":{"enabled":true,"compact":true}}` on its own connection.
This reduces a 32 × 8 RGB preview from 1,551 JSON bytes to 1,041 JSON bytes
with lossless Base64 encoding. The preview schedule stays the same. Legacy
clients and ESP8266 continue to receive the hexadecimal `liveview` message.
The embedded WebUI controller expands compact frames before the existing
WebUI message handler receives them. The format selection resets on reconnect.

The ESP32 can queue a compact frame while up to one compact frame of TCP data
awaits acknowledgement. It still requires room for the entire next message,
an empty application queue, and no unsent TCP segments. This bounds the
preview's contribution to in-flight data to two frames. Legacy previews
still require acknowledgement of the previous data.

## Screen transitions

Send a screen payload with `"switchAnimation": {"aktiv": true, "animation": "curtain"}`.
The new names are `curtain`, `pixelDissolve`, `slide`, `blinds`, `crt` and `glitch`.
They apply to incoming screen content, including a clock command. With screen
content, `random` chooses among the six new effects, Fade, Colored Bar Wipe
and Zig Zag Wipe. Effect-only requests keep the three legacy choices.
Bitmap Wipe is selected explicitly with its pixel data. Clock Auto Fallback has
a separate numeric setting, `clockAutoFallbackAnimation`. Values 0–4 retain
their meanings (None, Fade, Colored Bar Wipe, ZigZag Wipe, Random Wipe). Values
5–10 select Curtain, Pixel Dissolve, Slide, Blinds, CRT and Glitch respectively.
Random Wipe chooses among all nine effects that can render the clock with no
extra payload. Deploy the matching WebUI bundle with the firmware.

The new renderer uses one fixed 768-byte RGB snapshot on both platforms. It
composites into the existing LED buffer and renders at most one step per service
call, skipping overdue intermediate steps over a nominal 384 ms. No request
storage survives the handler. Bitmap and scrolling playback start from the
prepared frame after the transition. Another screen can replace an in-progress
transition, and sleep, clock fallback and firmware upload can take over the display.

Existing HTTP, MQTT and WebSocket screen commands accept the new names. The
Node-RED Switch Animation editor exposes the six styles in its dropdown. The WebUI exposes the new clock fallback choices in Options. PixelIt.API requires no changes for this
payload. Add the names, the random selection, an example with screen content and the
distinction from clock fallback to the Switch Animation section of external PixelIt.Docs before
stable release.

Run `python3 test/animation/transitions.py` to check the transition renderer with
simulated time and a serpentine matrix. Check visuals, frame intervals, dropped
LED frames, minimum ESP8266 free heap and largest free block with repeated screen
changes and concurrent network traffic on a designated device.


## Home Assistant topic isolation

Home Assistant discovery enables the existing per-device MQTT topics even when
`mqttUseDeviceTopic` is off. The effective setting applies to subscriptions,
periodic and on-demand states, button events and the Last Will, so discovery
cannot bind a device to another display's status on a shared master topic.
Master-topic publications and commands remain available. The broker's single
Last Will uses the device topic whenever device topics are active. Discovery
keeps the same entity IDs and refreshes retained configs on reconnect. Hostnames
must be unique on the broker. No new payload buffers or publishing intervals
are introduced; installations previously using only master topics now also send
device-specific updates when discovery is enabled.

Update external PixelIt.Docs for the release to explain this behavior and the
unique-hostname requirement. WebUI, Node-RED and PixelIt.API require no changes.


### Sleep screen colors

The `zZz` sleep screen uses the same violet-to-pink gradient as `REBOOT`.
Each existing letter stage recolors only the lit pixels in the logical text
area through the existing LED buffer. Letter positions, delays, fade-out, and
wake behavior are unchanged. The boot screen reuses the recoloring helper and
keeps its white `IT` and untouched classic option. No setting, client payload,
pixel buffer or periodic work was added. `python3 test/startup/run.py` checks
both boot styles and the sleep gradient, stage delays and wake request.
Document the new appearance in the Sleep Mode section of external
PixelIt.Docs `src/api.md` with the matching stable firmware. No companion
client code changes are required.

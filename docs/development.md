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
per second. On ESP8266, previews use WebSocket fragments queued over multiple
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

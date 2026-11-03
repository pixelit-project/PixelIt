# Bitmap animations

[Back to the README](../README.md) · [Build and test instructions](development.md)

The extended limits below are part of the unreleased development firmware.
For released versions, check the [changelog](../CHANGELOG.md).

## Sizes and limits

`bitmapAnimation.data` contains one array per frame. Each frame must contain
exactly `width × height` RGB565 values in the range 0–65535, ordered row by row
from left to right, starting at the top.

ESP8266 keeps its 1,792-pixel / 28-frame budget. ESP32 has a separate
10,240-pixel / 160-frame budget, without requiring PSRAM:

| Frame size (width × height) | ESP8266 | ESP32 |
| --- | ---: | ---: |
| 8×8 (default) | 28 | 160 |
| 16×8 | 14 | 80 |
| 32×8 (full display) | 7 | 40 |

Empty, malformed and oversized animations are rejected before playback.

## Sending an animation

Set `bitmapAnimation.size` to `{"width":32,"height":8}` for the full display,
with 256 values per frame. The [32×8 example](../examples/animation-32x8.json)
contains seven complete frames and can be sent as-is to HTTP `/api/screen`
or an MQTT `setScreen` topic.

For WebUI WebSocket commands, wrap the screen object:

```json
{
  "setScreen": {
    "bitmapAnimation": {
      "size": { "width": 2, "height": 1 },
      "position": { "x": 0, "y": 0 },
      "data": [[65535, 0], [0, 65535]],
      "animationDelay": 200,
      "limitLoops": 0,
      "rubberbanding": false
    }
  }
}
```

This small example uses two pixels per frame to keep the format readable.
`animationDelay` is in milliseconds; `limitLoops: 0` repeats indefinitely.
Successful WebSocket `setScreen` commands acknowledge processing with
`{"response":"OK"}`.

## Transport limits

- ESP8266: MQTT packets, including topic/overhead, must fit in 11,264 bytes; HTTP bodies and WebSocket commands in 12,288 bytes.
- ESP32: MQTT packets, including topic/overhead, must fit in 65,535 bytes; HTTP bodies and WebSocket commands also have a 65,535-byte limit.
- Large metadata or memory pressure can exhaust bounded receive storage before the wire limit. Upload success still depends on available memory.
- Prefer compact JSON and send large uploads sequentially. Concurrent large transfers and multiple live viewers can exhaust ESP8266 memory.

## Playback diagnostics

`/api/matrixinfo` reports animation capacity, free memory and MQTT status.
`/api/animationinfo` reports:

- `animationDelay`, `animationRenderedFrames` and `animationMaxFrameGap` for playback timing.
- `animationFailedLedFrames`, plus ESP8266 `ledFramesSent` and `ledFrameRetries`, for LED transmission diagnostics. Hardware transfer counters are available on ESP8266.
- `slowestTask` and `slowestTaskMillis` for identifying slow background tasks.

Frame statistics reset when a new animation is loaded; LED transfer counters
cover the device's uptime. A browser preview samples the display and can skip
images when the browser or network is slow. Use the firmware counters to check
actual LED delivery on ESP8266. ESP32 has no failed-transfer instrumentation;
its zero `animationFailedLedFrames` value does not prove physical delivery.

# Bitmap animations

[Back to the README](../README.md) · [Build and test instructions](development.md)

The extended limits below are part of the unreleased development firmware.
For released versions, check the [changelog](../CHANGELOG.md).

## Sizes and limits

`bitmapAnimation.data` contains one array per frame. Each frame must contain
exactly `width × height` RGB565 values in the range 0–65535, ordered row by row
from left to right, starting at the top.

All builds and transports share a budget of 1,792 pixels and a maximum of 28 frames:

| Frame size (width × height) | Maximum frames |
| --- | ---: |
| 8×8 (default) | 28 |
| 16×8 | 14 |
| 32×8 (full display) | 7 |

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

- MQTT: the complete packet, including topic, JSON and protocol overhead, must fit in 11,264 bytes.
- HTTP and WebSocket screen messages: maximum 12,288 bytes.
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
actual LED delivery.

# PixelIt.WebUI agent guidance

This repository contains the Vue 2 device WebUI. Read `README.md` and
`package.json` for local setup and supported commands.

## Shared project requirements

Smooth device animations and ESP8266 memory compatibility are hard requirements.
Evaluate changes against the smallest supported device, not just ESP32 or a desktop.
Keep device-bound payloads, frame counts, dimensions, request frequency and concurrent
transfers bounded. Avoid unnecessary polling, retries or duplicate transfers that
consume firmware heap or interrupt playback. Do not raise firmware limits implicitly.
Consult the target firmware version's `include/AnimationLimits.h` and
`docs/development.md`; avoid copying limits here where they can become stale.
Builds and host tests alone do not prove smooth playback or safe device RAM usage.
For changes affecting device load, report whether ESP8266 timing and peak-memory
validation was performed, and identify any remaining gap.

## Related repositories

- [PixelIt](https://github.com/pixelit-project/PixelIt): device firmware and device-side HTTP/MQTT/WebSocket contracts.
- [PixelIt.Docs](https://github.com/pixelit-project/PixelIt.Docs): project and user documentation.
- [node-red-contrib-pixelit](https://github.com/pixelit-project/node-red-contrib-pixelit): Node-RED control nodes.
- [PixelIt.WebUI](https://github.com/pixelit-project/PixelIt.WebUI): device WebUI.
- [PixelIt.API](https://github.com/pixelit-project/PixelIt.API): bitmap API service, separate from the device API.

Inspect relevant companion code when changing shared formats or behavior. Preserve
compatibility with existing clients and supported firmware; explicitly identify
required companion changes and external documentation updates. Report unavailable
sources rather than assuming their behavior.

## External API access

Use PixelIt.API whenever it supports the required functionality, including release
metadata, to benefit from its caching and avoid upstream rate limits. Reuse data
already loaded by the WebUI rather than making duplicate requests. If PixelIt.API
does not provide what is needed, discuss the gap and proposed solution with the
user before implementing an alternative service, direct upstream API request, or
fallback. Device-local HTTP and WebSocket operations still use the device API.

## WebUI conventions and validation

- Change Vue source here; the firmware embeds generated HTML in
  `include/Webinterface.h`. Do not substitute manual edits to that generated file.
- Preserve HTTP and WebSocket contracts, persisted settings and existing workflows.
  Validate device-bound input before sending it; handle firmware rejections clearly.
- Bound live-preview traffic and clean up timers, listeners and sockets when views
  close. Avoid concurrent uploads and redundant sends during animation playback.
- Keep the production bundle suitable for firmware embedding; assess added assets
  and dependencies for size as well as browser performance.
- For code changes run relevant scripts in `tests/` with Node, plus `npm run build`.
  Check affected UI flows in the browser, including reconnects and invalid input.
  Use `npm run lint` when relevant; review any automatic formatting changes.
- Documentation-only edits do not require application builds. Report checks and gaps.

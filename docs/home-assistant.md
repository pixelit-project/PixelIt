# Home Assistant MQTT discovery

Enable MQTT and Home Assistant discovery in the PixelIt configuration, then
restart the device after installing the firmware. Home Assistant's MQTT integration
must use the same broker and the default discovery prefix `homeassistant`.

In addition to the matrix light, installed sensors, button states and WLAN
information, the device offers:

| Entity | Function |
| --- | --- |
| Automatic brightness | Enable or disable ambient-light brightness control |
| Display text | Prepare a message without changing the current display |
| Text color | Set the color as `#RRGGBB`, e.g. `#FF8000` |
| Text color picker | Native HA RGB color wheel, synchronized with the HEX field |
| Firmware | Installed/latest version and release link (read-only update entity) |
| Text font size | `Small` or `Large` |
| Text alignment | `Centered` or `Left` |
| Text scrolling | `Auto`, `Always` or `Off` |
| Show text | Apply the prepared text and settings together; wakes the matrix |
| Automatic clock | Enable the clock fallback after inactivity |
| Clock fallback delay | Set the inactivity period from 1 to 86400 seconds |
| Uptime | Seconds since boot |
| Free memory | Available heap in bytes |
| Restart reason | Firmware-reported reset reason |
| Button event (per enabled button) | `press`, `release`, `single_press`, `double_press`, `long_press` |

Uptime, free memory and restart reason are diagnostic entities, initially disabled.
Enable them in the device's entity list when needed. Clock settings are persisted;
the automatic-brightness switch changes the runtime setting, like `setScreen`.

In Home Assistant, set **Display text**, **Text color**, **Text font size**,
**Text alignment** and **Text scrolling**, then press **Show text**. Editing a field
only prepares the next message; a running animation continues until Show text is
pressed. The defaults are white, small font, centered alignment and automatic
scrolling. Vertical alignment is centered using the chosen font's glyph bounds,
including umlauts and descenders. Odd-height text on an even-height matrix leaves
one extra blank row on one side.

These settings and the prepared message are runtime values and reset on reboot;
editing them does not write flash. Text commands are processed only after the
large MQTT receive buffer has been released on ESP8266. If the full animation
receive buffer cannot be allocated, MQTT service falls back to a smaller buffer
so small control commands and keepalive can continue; full-size MQTT animation
messages still require the larger buffer. All fields report the device's accepted values
via `matrixinfo`. The native color wheel is exposed as an additional MQTT RGB
light named **Text color picker**. Open its more-info dialog to select a color;
the **Text color** HEX field stays synchronized. Its off switch stages black;
on restores the last nonblack color. These controls only prepare the text color
and do not switch the matrix itself or interrupt playback. Press Show text to
apply the selected color to the display.
Text accepts up to 255 UTF-8 bytes and uses the existing PixelIt font. Show text
respects an active forced screen. An empty message clears the text when shown.
No bitmap, gallery API access or additional HA automation is required.

The discovered fields publish plain UTF-8 values on
`<topic>/setTextOptions/text`, `/color`, `/font`, `/alignment` and `/scroll`.
Quotes and umlauts need no JSON escaping on these topics. This is also the
recommended interface for MQTT clients.

For combined updates, send UTF-8 JSON to `<topic>/setTextOptions`, for example:

```json
{"text":"Hallo","color":"#FF8000","font":"Small","alignment":"Centered","scroll":"Auto"}
```

Each property is optional; invalid updates are rejected without partial changes.
The legacy ArduinoJson parser does not decode `\uXXXX` sequences; use literal
UTF-8 for non-ASCII characters in combined JSON updates.
Send the literal `PRESS` to `<topic>/showText` to display the prepared message.
The existing `<topic>/setText` command still accepts plain text and displays it
immediately using the current text options. Do not retain command messages.
The reported `displayText` is the last text accepted for rendering, including
messages received through `setScreen`; `textMessage` is the prepared HA message.
Both can remain populated after the display returns to a clock or another screen.

Buttons are debounced for 30 ms. A short click is reported after a 350 ms window;
two short releases within that window report `double_press` instead. Holding a
button for 800 ms reports `long_press` once, without a short-click event for that
hold. The existing local button action still runs on each debounced press.
Events are not retained, so MQTT reconnections do not replay button actions.

Discovery and current states are sent on MQTT reconnect, on Home Assistant's
`homeassistant/status = online` message, and after configuration updates. Discovery configurations and initial states are
sent one at a time, with a pause and allowance for the next animation frame.
MQTT configuration writes run after the large receive buffer has been released
on ESP8266, leaving memory available for configuration serialization.
Disabled sensors/buttons and disabled discovery remove the corresponding retained
configurations. Existing entity IDs are preserved. When using the device topic,
its availability topic also receives the broker's Last Will on connection loss.

For an automation, set the discovered text entity with `text.set_value`, then
press the discovered Show text button with `button.press`, or watch
the button event entity's state changes and filter its `event_type` attribute.
For example, `single_press` could send a message, while `long_press` toggles the
matrix light. Avoid also triggering on `press` unless both actions are intended.

## Verification

```sh
python3 test/homeassistant/run.py
pio run -e ESP8266_d1_mini -e ESP32_generic
```

On hardware, check discovery after an HA restart, disconnect the device to verify
unavailability, toggle automatic brightness, send text containing quotes and
umlauts, adjust clock fallback, and test each button gesture. Disable a sensor or
button and verify its entities disappear after the configuration is applied.

To measure playback while publishing discovery three times on a physical device:

```sh
python3 test/homeassistant/device_timing.py http://192.168.0.40
```

Use a test device with the new firmware and MQTT discovery enabled, and pause
other clients that replace its screen. The test temporarily displays an animation,
writes the existing clock delay three times (without changing its value), counts
this device's discovery publications on the broker, and restores the clock.
The MQTT broker must be reachable from the computer running the test.

## Device validation — 2026-10-31

Measured on an ESP8266 D1 mini at 160 MHz with seven 32×8 frames and a target
frame interval of 150 ms. Other screen-writing clients were paused for the tests.

| Test | Frames | Maximum frame gap | Failed LED frames |
| --- | --- | --- | --- |
| Existing 2.7.0-beta.1 firmware, about 60 s | 401 | 154 ms | 0 |
| Final firmware, normal playback, about 60 s | 401 | 151 ms | 0 |
| Paced discovery plus three config saves, about 30 s | 199 | 241 ms | 0 |

The broker confirmed all 75 discovery configuration publications in the stress
run; uptime remained continuous and the persisted configuration passed the
completeness checks. Configuration saving still causes occasional short delays.
These measurements do not guarantee identical timings with other WiFi/broker
conditions, sensors, or concurrent WebSocket clients.

The first implementation sent a whole discovery batch synchronously. The device
test exposed a 1541 ms frame gap and a subsequent exception during repeated
configuration/discovery activity. That implementation was replaced by the paced
scheduler and deferred MQTT configuration processing. The failed intermediate
run also left a partial saved configuration; a later full save restored its
structure from the then-current runtime values. Original custom values for
fields omitted by that partial save could not be verified retrospectively.


### Text control verification

```sh
python3 test/homeassistant/device_text.py http://192.168.0.40
```

The physical-device check exercises staged changes during animation, quotes and
umlauts, both font sizes and alignments, scrolling modes, invalid color rejection,
the Show text button, and verifies that configuration is not written. It restores
the previous runtime text options and the clock afterwards.


## Firmware update information

**Firmware** is a standard MQTT update entity with the installed version, the
latest newer stable version and a link to the project's release notes. It has
no install command. Downloads and installation are still performed through the
existing firmware update interface.

The device uses its existing version API, first checking after roughly 30 seconds
and then every eight hours. Checks are deferred while an animation or scrolling
text is active; the HTTP connection is closed immediately after each response
to release its network buffers. MQTT discovery receives update information even when the visual
update reminder is disabled. Before the first successful check, the entity uses
the installed version as its latest version. `updateCheckCompleted` in matrixinfo
indicates whether a successful check has occurred since boot. A stable release
older than the installed firmware does not produce a downgrade notification.

### Update information and color picker validation — 2026-10-31

The final ESP8266 D1 mini build confirmed both retained discovery entities at the
broker, synchronized the picker and HEX field in both directions, and restored
the last nonblack color after off/on. The version API returned `2.6.0` for the
local `0.0.0-beta` test build. The configuration remained unchanged in the text
control test. A subsequent 30-second animation run rendered 200 frames at a
150 ms target interval, with a maximum gap of 259 ms (WebSocket work was present),
zero failed LED transfers and no reboot. The clock was restored after testing.
ESP8266, ESP32 and Ulanzi builds and all host regression tests passed.

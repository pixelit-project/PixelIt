# PixelIt.WebUI

Creator and Test Area use the connected firmware's advertised animation frame and
pixel budgets. Older firmware falls back to 28 frames / 1,792 pixels; ESP32 builds
can advertise larger limits. Reconnects clear the previous device limits.
Demo mode advertises the ESP32 budgets from the companion firmware's
`include/AnimationLimits.h` and `include/ScreenLimits.h` for Creator, Test Area
and Gallery compatibility checks.
Saving icons to the Gallery database is disabled in demo mode.

This is the device WebUI for the awesome [PixelIt Project](https://github.com/pixelit-project/PixelIt).

## How to Develop?

1. **Clone the Repository**
   - Clone the project repository to your local machine:
     ```bash
     git clone https://github.com/pixelit-project/PixelIt.WebUI.git
     ```

2. **Navigate to the Project Directory**
   - Change into the project directory:
     ```bash
     cd PixelIt.WebUI
     ```

3. **Install Dependencies**
   - Install the necessary dependencies using `npm`:
     ```bash
     npm install
     ```

4. **Set Up Environment Variables**
   - Copy the example environment file to a new `.env.local` file:
     ```bash
     cp .env.local.example .env.local
     ```
   - On Windows, use:
     ```bash
     copy .env.local.example .env.local
     ```
   - Edit the `.env.local` file to suit your specific needs.

5. **Start the Development Server**
   - Run the following command to start the development server:
     ```bash
     npm run serve
     ```
   - On Windows, if applicable, use:
     ```bash
     npm run serve-win
     ```
   - The server will start on port `8080`. You can access it at `http://localhost:8080`.

## Firmware release downloads

The update page reuses `/api/Releases` data from PixelIt.API. Direct firmware
links require `fwdownloads` entries containing `fileName` (original asset name)
and `downloadURL` (browser download URL). Deploy the corresponding PixelIt.API
extension before this WebUI. With older API deployments or unknown hardware,
the page shows release notes instead of guessing a firmware URL or querying the
GitHub API directly. Full-upgrade images are excluded from OTA download selection.

## Reporting gallery icons

The flag button in the gallery opens a report dialog. Users select a reason and may add details; details are
required for "Other reason". The combined reason and details (up to 1000 characters)
are sent to PixelIt.API's `POST /api/reports`; no login is required.
A moderator decides whether to hide the icon. Reports do not delete it immediately.
Deploy the API's moderation endpoints before this WebUI. Gallery reads and reports
use the configured `VUE_APP_API_SERVER` base URL. Reporting is disabled in demo mode.
No report requests are sent to the display and no background polling is added.

## Beta update searches

Options → Defaults → **Include beta releases in update searches** opts into
published prereleases for update hints and firmware downloads. Save the configuration
to apply it; the default is off. Requires firmware with `includePrereleases` support
and PixelIt.API with the `includePrereleases=true` release query. Older APIs keep
returning stable releases. No additional release polling is introduced.

## Gallery device compatibility

The Gallery defaults to images and animations that fit the connected device's
advertised dimensions, frame and pixel budgets. **Also show images incompatible with this device** reveals the remaining entries with a centered status label and disabled Send button.
Labels distinguish incompatible content from invalid image data; hover for the
reason or expand the label with a click, tap or keyboard.
Cards show dimensions and actual frame counts. Offline browsing and previews remain
available; sending waits for a connected device and fresh system information.
Older firmware uses the conservative 28-frame / 1,792-pixel fallback.

Compatibility is checked again when sending, including pixel data and message size.
Frames are never trimmed automatically. Only small metadata is cached while browsing;
pixel arrays are parsed on send. No additional API requests or device polling are
introduced. This uses existing API records and firmware system information, so no
API schema change is required. Include the Gallery behavior in PixelIt.Docs when
this WebUI is released; its published user documentation currently targets stable releases.


Gallery details offer **Copy RGB565 data**, including the HTTP clipboard fallback.
Successful Creator uploads invalidate the browser Gallery cache; the next Gallery
visit fetches the current list once. Failed uploads preserve the cache and drawing.
Deploy the companion API cache-invalidation change to make new uploads visible
immediately rather than waiting for the API cache to expire. Include these controls
in the stable Gallery/Creator user guide when releasing the WebUI.

## MQTT sensor publishing (development firmware)

Options exposes sensor publishing controls when the connected firmware advertises
`config.mqttSensorPublish`. Older firmware shows a firmware-update hint instead of editable controls. Event mode checks
each sample and optionally limits send frequency; periodic mode checks at the
configured interval. Each measurement has its own change threshold, compared with
the last successfully published sensor payload. Temperature uses the configured
°C/°F unit, humidity and battery use percentage points, pressure hPa, gas kΩ and
illuminance lx. Any qualifying measurement publishes its whole topic payload;
sensor and lux topics have independent baselines. Zero thresholds accept any
change, while zero minimum/maximum intervals disable the additional interval.
A maximum silent interval also republishes unchanged readings. Initial connection,
Home Assistant discovery and forced publications bypass normal filtering.

Intervals are whole seconds (check interval 3–86400, minimum/maximum 0–86400).
A nonzero maximum must be at least the active check/minimum interval. Thresholds
must be finite numbers from 0 to 1000000. Invalid settings cannot be sent by the
form. The firmware preserves previous publishing settings on invalid input.
Sensor sampling and WebSocket updates retain their existing cadence, so timings
are evaluated on the next available sample rather than exact wall-clock deadlines.

Run `node tests/sensor-publish.test.cjs` and `npm run build`. The matching WebUI
bundle must be published and embedded by the firmware pipeline before delivery.
ESP8266 timing and peak-memory validation on hardware remains required.


## Boot animation

Under Options → Defaults → **Boot animation**, select **Gradient letters**
(the default) or **Multicolor letters** (the original per-letter colors).
Enable **Boot-Screen active** to use the selected animation. Save the
configuration to apply the selection on restart. The selector appears on
firmware that reports a supported `bootScreenStyle` value.

## Startup WiFi screen

Options → Defaults → **Show WiFi screen during startup** controls the entire
connection screen (label and moving trail), including the hotspot notice. It is
independent of Bootscreen active and defaults to enabled. Save configuration to
apply it at the next startup. Requires firmware advertising the boolean
`wifiScreenAktiv`; older firmware shows an update hint instead of an editable
control. Deploy the matching production bundle before regenerating the firmware's
embedded index, so its hashed asset URLs exist on the WebUI host.


## Preview visibility

On firmware advertising `sysinfo.previewControl: true`, hidden pages pause their
own preview using `{ "preview": { "enabled": false } }`; visible pages resume with
`true` and receive the latest image. Controls, status and the physical display
remain active. Duplicate same-device sockets within one page share the newest
capable socket for previews; another device is independent. Older firmware keeps its existing behavior without receiving new
commands. An event-driven inline controller installs before the socket plugin;
there is no extra timer, pixel buffer or dependency. Reload existing tabs after
installing the matching firmware/index to activate visibility handling.

The standard production index inlines `public/preview-visibility.js`. For an
isolated device build retaining already-published app hashes, use
`node scripts/preview-compatible-index.cjs OLD_INDEX NEW_INDEX`, then generate the
firmware header through its `.github/webui.py`. This compatibility index changes
only the inline controller and preserves existing published script/style URLs;
it does not publish or substitute unreleased application assets.

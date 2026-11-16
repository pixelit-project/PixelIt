<img src=".github/logo.png" width="100px" height="100px" />

# PixelIt - The Matrix Display

![](.github/pixelit.png)

![GitHub Workflow Status (with event)](https://img.shields.io/github/actions/workflow/status/pixelit-project/PixelIt/build-and-release.yml) ![GitHub all releases](https://img.shields.io/github/downloads/pixelit-project/PixelIt/total) ![GitHub Repo stars](https://img.shields.io/github/stars/pixelit-project/PixelIt) ![GitHub commit activity (branch)](https://img.shields.io/github/commit-activity/t/pixelit-project/pixelit) ![GitHub commit activity (branch)](https://img.shields.io/github/commit-activity/y/pixelit-project/pixelit) ![GitHub top language](https://img.shields.io/github/languages/top/pixelit-project/pixelit)

PixelIt turns an ESP8266/ESP32 with a WS2812B LED matrix or an Ulanzi TC001 into a
pixel art display for images, animations, scrolling text and clocks. Control it
through the WebUI, HTTP JSON API, MQTT or WebSocket, or connect it to Node-RED,
Home Assistant and ioBroker.

If you want to get an impression of the WebUI, you can do that here in [Demo WebUI](https://pixelit-project.github.io/PixelIt/webui/) :rocket:

-   :memo: [Documentation](https://pixelit-project.github.io/)
-   :green_heart: [PixelIt Icon Gallery (in Demo WebUI)](https://pixelit-project.github.io/PixelIt/webui/#/gallery)
-   :bulb: [GitHub Discussions (Forum)](https://github.com/pixelit-project/PixelIt/discussions) [![](https://img.shields.io/github/discussions/pixelit-project/PixelIt)](https://github.com/pixelit-project/PixelIt/discussions)
-   :speech_balloon: [Telegram Channel](https://t.me/pixelitdisplay) [![](https://img.shields.io/endpoint?style=flat&url=https%3A%2F%2Frunkit.io%2Fdamiankrawczyk%2Ftelegram-badge%2Fbranches%2Fmaster%3Furl%3Dhttps%3A%2F%2Ft.me%2Fpixelitdisplay)](https://t.me/pixelitdisplay)
-   :speech_balloon: [Discord Channel](https://discord.gg/ERBSHWxB2S) [![](https://img.shields.io/discord/1145731525996970025?logo=discord)](https://discord.gg/ERBSHWxB2S)

## Project highlights

-   :fire: Support for **[Node-RED](https://pixelit-project.github.io/nodered.html)**, **[HomeAssistant](https://pixelit-project.github.io/homeassistant.html#pixelit-custom-integration)** and **[ioBroker](https://pixelit-project.github.io/iobroker.html)**
-   :fire: Support for **[DIY matrices](https://pixelit-project.github.io/hardware.html#parts-list)** (ESP32/ESP8266) and prebuilt **[Ulanzi TC001 Pixel Clock](https://www.ulanzi.com/products/ulanzi-pixel-smart-clock-2882?aff=1181)**\*
-   :fire: Awesome [WebUI](https://pixelit-project.github.io/PixelIt/webui/) for configuration and control. Check out the **[Demo](https://pixelit-project.github.io/PixelIt/webui/)**
-   :fire: Great **[documentation](https://pixelit-project.github.io/)**
-   :fire: Unlimited possibilities through the open **[API](https://pixelit-project.github.io/api.html)**
-   :fire: Helpful and welcoming community on **[Telegram](https://t.me/pixelitdisplay)**, **[Discord](https://discord.gg/ERBSHWxB2S)** and **[GitHub Discussions](https://github.com/pixelit-project/PixelIt/discussions)**
-   :fire: Showroom for your own PixelIt builds on the **[Show your PixelIt](https://github.com/pixelit-project/PixelIt/discussions/48)** thread.

## Getting started

1. Choose your hardware and follow the [setup documentation](https://pixelit-project.github.io/hardware.html).
2. Use the [Webflasher and installation guide](https://pixelit-project.github.io/firmware.html), or download the matching board image from [GitHub Releases](https://github.com/pixelit-project/PixelIt/releases).
   For ESP32 USB installation, use the `.full-upgrade.bin` image; for WebUI OTA updates, use the application `.bin` without `.full-upgrade`.
3. Complete WiFi setup, then open the device's IP address in your browser.
4. Configure the display in the WebUI, browse the icon gallery, or create your own bitmaps and animations in Pixel Creator.

Firmware builds are available for ESP8266 generic, D1 mini and NodeMCU v2,
and ESP32 generic, D1 mini32 and Ulanzi TC001. Choose the image for your exact board.

## Control and integrations

- **WebUI:** device settings, live preview, icon gallery, bitmap and animation editor, configuration backup and firmware updates.
- **HTTP, MQTT and WebSocket:** send screens, text, bitmaps and animations through the [JSON API](https://pixelit-project.github.io/api.html).
- **Home Assistant:** use the [custom integration](https://pixelit-project.github.io/homeassistant.html#pixelit-custom-integration) for direct WebSocket control, playlists and a liveview card, or enable firmware MQTT discovery for display, text, clock, sensor and button entities. See the [MQTT discovery reference](docs/home-assistant.md) for setup and topic details.
- **Node-RED:** build display flows with [node-red-contrib-pixelit](https://flows.nodered.org/node/node-red-contrib-pixelit).
- **ioBroker:** connect through the [PixelIt adapter](https://pixelit-project.github.io/iobroker.html).

Screen transitions, an automatic clock fallback and ambient-light brightness
control are configurable; sensors and physical buttons depend on the installed hardware.

## Bitmap animations

Since firmware **3.0.0**, animation capacity depends on the device:

| Frame size (width × height) | ESP8266 maximum frames | ESP32 maximum frames |
| --- | ---: | ---: |
| 8×8 | 28 | 160 |
| 32×8 (full display) | 7 | 40 |

These limits apply to HTTP, MQTT and WebUI uploads. Payload size and available
memory can further limit uploads. See [animation usage and examples](docs/animations.md)
for the JSON format, transport limits and playback diagnostics.

## Releases and development

Use [GitHub Releases](https://github.com/pixelit-project/PixelIt/releases/latest)
for stable firmware and the [changelog](CHANGELOG.md) for version-specific changes.
The optional beta update channel is disabled by default. This branch can contain
unreleased changes; the optional sleep-animation setting is currently unreleased.

When upgrading from firmware before 3.0.0 with Home Assistant MQTT discovery,
device topics are now always enabled. Availability automations that previously
used the master topic must use `<master>/<hostname>/state`.

- [Build and test instructions](docs/development.md)
- [Firmware source](https://github.com/pixelit-project/PixelIt) (this repository)
- [WebUI source](https://github.com/pixelit-project/PixelIt.WebUI)
- [User documentation source](https://github.com/pixelit-project/PixelIt.Docs)
- [Bitmap API service](https://github.com/pixelit-project/PixelIt.API)

## Contributors

Thanks to these great people for supporting this project.

<!-- readme: contributors -start -->
<table>
	<tbody>
		<tr>
            <td align="center">
                <a href="https://github.com/o0shojo0o">
                    <img src="https://avatars.githubusercontent.com/u/19333515?v=4" width="100;" alt="o0shojo0o"/>
                    <br />
                    <sub><b>o0shojo0o</b></sub>
                </a>
            </td>
            <td align="center">
                <a href="https://github.com/foorschtbar">
                    <img src="https://avatars.githubusercontent.com/u/10727275?v=4" width="100;" alt="foorschtbar"/>
                    <br />
                    <sub><b>foorschtbar</b></sub>
                </a>
            </td>
            <td align="center">
                <a href="https://github.com/miccgn">
                    <img src="https://avatars.githubusercontent.com/u/71180164?v=4" width="100;" alt="miccgn"/>
                    <br />
                    <sub><b>miccgn</b></sub>
                </a>
            </td>
            <td align="center">
                <a href="https://github.com/rliegmann">
                    <img src="https://avatars.githubusercontent.com/u/16445478?v=4" width="100;" alt="rliegmann"/>
                    <br />
                    <sub><b>rliegmann</b></sub>
                </a>
            </td>
            <td align="center">
                <a href="https://github.com/jekader">
                    <img src="https://avatars.githubusercontent.com/u/2088123?v=4" width="100;" alt="jekader"/>
                    <br />
                    <sub><b>jekader</b></sub>
                </a>
            </td>
            <td align="center">
                <a href="https://github.com/Metaln00b">
                    <img src="https://avatars.githubusercontent.com/u/25951169?v=4" width="100;" alt="Metaln00b"/>
                    <br />
                    <sub><b>Metaln00b</b></sub>
                </a>
            </td>
		</tr>
		<tr>
            <td align="center">
                <a href="https://github.com/pplucky">
                    <img src="https://avatars.githubusercontent.com/u/16407309?v=4" width="100;" alt="pplucky"/>
                    <br />
                    <sub><b>pplucky</b></sub>
                </a>
            </td>
            <td align="center">
                <a href="https://github.com/fmunozs">
                    <img src="https://avatars.githubusercontent.com/u/1041604?v=4" width="100;" alt="fmunozs"/>
                    <br />
                    <sub><b>fmunozs</b></sub>
                </a>
            </td>
            <td align="center">
                <a href="https://github.com/rozza-m">
                    <img src="https://avatars.githubusercontent.com/u/1784953?v=4" width="100;" alt="rozza-m"/>
                    <br />
                    <sub><b>rozza-m</b></sub>
                </a>
            </td>
            <td align="center">
                <a href="https://github.com/Flolant">
                    <img src="https://avatars.githubusercontent.com/u/103022070?v=4" width="100;" alt="Flolant"/>
                    <br />
                    <sub><b>Flolant</b></sub>
                </a>
            </td>
            <td align="center">
                <a href="https://github.com/hermannbach">
                    <img src="https://avatars.githubusercontent.com/u/6616722?v=4" width="100;" alt="hermannbach"/>
                    <br />
                    <sub><b>hermannbach</b></sub>
                </a>
            </td>
            <td align="center">
                <a href="https://github.com/d4rkd3v1l">
                    <img src="https://avatars.githubusercontent.com/u/13336238?v=4" width="100;" alt="d4rkd3v1l"/>
                    <br />
                    <sub><b>d4rkd3v1l</b></sub>
                </a>
            </td>
		</tr>
		<tr>
            <td align="center">
                <a href="https://github.com/hamster65">
                    <img src="https://avatars.githubusercontent.com/u/36369379?v=4" width="100;" alt="hamster65"/>
                    <br />
                    <sub><b>hamster65</b></sub>
                </a>
            </td>
		</tr>
	<tbody>
</table>
<!-- readme: contributors -end -->

## WebUI

![](.github/webui.png)

## Videos

[![](.github/youtube.jpg)](https://www.youtube.com/watch?v=Tpol4iNq4xo&pp=ygUHcGl4ZWxpdA%3D%3D)

## Disclaimer

\* This link and some others in the documentation are affiliate links. We would be happy if you use this link, but of course you don't have to.

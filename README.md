<img src=".github/logo.png" width="100px" height="100px" />

# PixelIt - The Matrix Display

![](.github/pixelit.png)

![GitHub Workflow Status (with event)](https://img.shields.io/github/actions/workflow/status/pixelit-project/PixelIt/build-and-release.yml) ![GitHub all releases](https://img.shields.io/github/downloads/pixelit-project/PixelIt/total) ![GitHub Repo stars](https://img.shields.io/github/stars/pixelit-project/PixelIt) ![GitHub commit activity (branch)](https://img.shields.io/github/commit-activity/t/pixelit-project/pixelit) ![GitHub commit activity (branch)](https://img.shields.io/github/commit-activity/y/pixelit-project/pixelit) ![GitHub top language](https://img.shields.io/github/languages/top/pixelit-project/pixelit)

The PixelIt is an ESP8266/ESP32 and WS2812B-LED based PixelArt display, controlled and fed via a JSON API.
Settings and small tests are possible via the web interface,
also a Node-RED node ([node-red-contrib-pixelit](https://flows.nodered.org/node/node-red-contrib-pixelit)) for the JSON API is available.

If you want to get an impression of the WebUI, you can do that here in [Demo WebUI](https://pixelit-project.github.io/PixelIt/webui/) :rocket:

-   :memo: [Documentation](https://pixelit-project.github.io/)
-   :green_heart: [PixelIt Icon Gallery (in Demo WebUI)](https://pixelit-project.github.io/PixelIt/webui/#/gallery)
-   :bulb: [GitHub Discussions (Forum)](https://github.com/pixelit-project/PixelIt/discussions) [![](https://img.shields.io/github/discussions/pixelit-project/PixelIt)](https://github.com/pixelit-project/PixelIt/discussions)
-   :speech_balloon: [Telegram Channel](https://t.me/pixelitdisplay) [![](https://img.shields.io/endpoint?style=flat&url=https%3A%2F%2Frunkit.io%2Fdamiankrawczyk%2Ftelegram-badge%2Fbranches%2Fmaster%3Furl%3Dhttps%3A%2F%2Ft.me%2Fpixelitdisplay)](https://t.me/pixelitdisplay)
-   :speech_balloon: [Discord Channel](https://discord.gg/ERBSHWxB2S) [![](https://img.shields.io/discord/1145731525996970025?logo=discord)](https://discord.gg/ERBSHWxB2S)

## Project highlights

-   :fire: Support for **[Node-RED](https://pixelit-project.github.io/nodered.html)**, **[HomeAssistant](https://pixelit-project.github.io/webinterface.html#mqtt)** and **[ioBroker](https://pixelit-project.github.io/iobroker.html)**
-   :fire: Support for **[DIY Matrixes](https://pixelit-project.github.io/hardware.html#parts-list)** (ESP32/ESP8266) and prebuild **[Ulanzi TC001 Pixel Clock](https://www.ulanzi.com/products/ulanzi-pixel-smart-clock-2882?aff=1181)**\*
-   :fire: Awesome [WebUI](https://pixelit-project.github.io/PixelIt/webui/) for configuration and control. Checkout the **[Demo](https://pixelit-project.github.io/PixelIt/webui/)**
-   :fire: Great **[documentation](https://pixelit-project.github.io/)**
-   :fire: Unlimited possibilities through the open **[API](https://pixelit-project.github.io/api.html)**
-   :fire: Helpful and welcoming community on **[Telegram](https://t.me/pixelitdisplay)**, **[Discord](https://discord.gg/ERBSHWxB2S)** and **[GitHub Discussions](https://github.com/pixelit-project/PixelIt/discussions)**
-   :fire: Showroom for your own PixelIt builds on the **[Show you PixelIt](https://github.com/pixelit-project/PixelIt/discussions/48)** thread.

## Getting started

1. Follow the [hardware and setup documentation](https://pixelit-project.github.io/).
2. Install the firmware for your board from [GitHub Releases](https://github.com/pixelit-project/PixelIt/releases).
3. Open the device's IP address in your browser to configure it and send images.

## Bitmap animations

The current development firmware supports these limits over HTTP, MQTT and WebUI:

| Frame size (width × height) | Maximum frames |
| --- | ---: |
| 8×8 | 28 |
| 16×8 | 14 |
| 32×8 (full display) | 7 |

See [animation usage and examples](docs/animations.md) for the JSON format,
transport limits and diagnostics. These changes are currently **unreleased**.

## Development and changes

- [Build and test instructions](docs/development.md)
- [Changelog](CHANGELOG.md)

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

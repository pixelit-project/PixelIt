![](https://raw.githubusercontent.com/o0shojo0o/ioBroker.pixelit/master/admin/pixelit.png)

# PixelIt

![](https://github.com/o0shojo0o/PixelIt/workflows/Build%20wemos_d1_mini32/badge.svg)
![](https://github.com/o0shojo0o/PixelIt/workflows/Build%20ESP8266/badge.svg)
![](https://github.com/o0shojo0o/PixelIt/workflows/Build%20d1_mini/badge.svg)  
![](https://github.com/o0shojo0o/PixelIt/workflows/Release%20wemos_d1_mini32/badge.svg)
![](https://github.com/o0shojo0o/PixelIt/workflows/Release%20ESP8266/badge.svg)
![](https://github.com/o0shojo0o/PixelIt/workflows/Release%20d1_mini/badge.svg)

The Pixel It is an ESP8266 / ESP32 (under construction) and WS2812B-LED based PixelArt display, controlled and fed via a JSON API.
Settings and small tests are possible via the web interface,
also a node-red node ([node-red-contrib-pixelit](https://flows.nodered.org/node/node-red-contrib-pixelit)) for the JSON API is available.

[Docu](https://docs.bastelbunker.de/pixelit/) |
[Forum](https://github.com/o0shojo0o/PixelIt/discussions) |
[Blog](https://www.bastelbunker.de/pixel-it/) |
[PixelIt Web](https://pixelit.bastelbunker.de/PixelGallery) |
[![](https://img.shields.io/endpoint?style=flat&url=https%3A%2F%2Frunkit.io%2Fdamiankrawczyk%2Ftelegram-badge%2Fbranches%2Fmaster%3Furl%3Dhttps%3A%2F%2Ft.me%2Fpixelitdisplay)](https://t.me/pixelitdisplay) |
[![](https://img.shields.io/discord/558849582377861122?logo=discord)](https://discord.gg/JHE9P9zczW)

## Contributors

Thanks to these great people for supporting this project.

<!-- readme: contributors -start -->
<table>
<tr>
    <td align="center">
        <a href="https://github.com/o0shojo0o">
            <img src="https://avatars.githubusercontent.com/u/19333515?v=4" width="100;" alt="o0shojo0o"/>
            <br />
            <sub><b>Dennis Rathjen</b></sub>
        </a>
    </td>
    <td align="center">
        <a href="https://github.com/miccgn">
            <img src="https://avatars.githubusercontent.com/u/71180164?v=4" width="100;" alt="miccgn"/>
            <br />
            <sub><b>Miccgn</b></sub>
        </a>
    </td>
    <td align="center">
        <a href="https://github.com/foorschtbar">
            <img src="https://avatars.githubusercontent.com/u/10727275?v=4" width="100;" alt="foorschtbar"/>
            <br />
            <sub><b>Fab!an</b></sub>
        </a>
    </td>
    <td align="center">
        <a href="https://github.com/rozza-m">
            <img src="https://avatars.githubusercontent.com/u/1784953?v=4" width="100;" alt="rozza-m"/>
            <br />
            <sub><b>Rozza-m</b></sub>
        </a>
    </td>
    <td align="center">
        <a href="https://github.com/hermannbach">
            <img src="https://avatars.githubusercontent.com/u/6616722?v=4" width="100;" alt="hermannbach"/>
            <br />
            <sub><b>HBach</b></sub>
        </a>
    </td>
    <td align="center">
        <a href="https://github.com/hamster65">
            <img src="https://avatars.githubusercontent.com/u/36369379?v=4" width="100;" alt="hamster65"/>
            <br />
            <sub><b>Hamster65</b></sub>
        </a>
    </td></tr>
<tr>
    <td align="center">
        <a href="https://github.com/pplucky">
            <img src="https://avatars.githubusercontent.com/u/16407309?v=4" width="100;" alt="pplucky"/>
            <br />
            <sub><b>Pplucky</b></sub>
        </a>
    </td></tr>
</table>
<!-- readme: contributors -end -->

## Changelog

<!--
### **WORK IN PROGRESS**
-->

### **WORK IN PROGRESS**

-   (o0shojo0o) update WebUi disallow decimals on 'Auto brightness' settings
-   (o0shojo0o) extended WebUi for 'Wifi Reset' and 'Factory Reset'
-   (o0shojo0o) added 'Wifi Reset' and 'Factory Reset' function for WebAPI and Socket
-   (o0shojo0o) fix hexColor for bar and bars
-   (rozza-m) added fat clock font [more](https://github.com/o0shojo0o/PixelIt/pull/169)
-   (pplucky) Improve MQTT discovery

### 0.3.19 (2022-04-24)

-   (o0shojo0o) fix weekday is not lighting if week is starting with monday (internal clock)
-   (o0shojo0o) extended WebUi for non-flashing time separator option
-   (rozza-m) remove leading zero from hours when in 12h mode (internal clock)
-   (rozza-m) provide option for non-flashing time separator (internal clock)

### 0.3.18 (2022-04-04)

-   (o0shojo0o) added option to change the start of the week (internal clock)
-   (o0shojo0o) added option to change date format (internal clock)
-   (hamster65) increase AM2320 sensor delay from 600 to 800ms

### 0.3.17 (2022-03-02)

-   (o0shojo0o) fixed wrong DST handling
-   (hermannbach) added support for BMP280 (only tested on D1 mini.)
-   (miccgn) memory optimization
-   (miccgn) added MQTT discovery information in HomeAssistant style
-   (miccgn) fix mqtt LWT
-   (miccgn) increase interval between reconnect attempts
-   (miccgn) added support for Max44009 lux sensor

### 0.3.16 (2022-01-21)

-   (miccgn) offer Zigzag animation and random animation for Clock Auto Fallback
-   (miccgn) added button actions for MP3 control
-   (miccgn) rearrange and fix WebUI
-   (miccgn) added support for local hardware buttons
-   (miccgn) added LDR smoothing
-   (miccgn) added support for BH1750 light sensor
-   (miccgn) added ZigZagWipe, bitmapWipe and randomization for wipes
-   (miccgn) fixed MP3Player
-   (miccgn) added possibility to set GL55xx type and resistor value in settings
-   (miccgn) added selectable pins for MP3Player and Sensors (ESP8266 only)
-   (miccgn) optimization of read BME680
-   (miccgn) added multiple Bitmaps

### 0.3.15 (2021-12-04)

-   (o0shojo0o) added offset options for gas sensor
-   (miccgn) added [BME680](https://docs.bastelbunker.de/pixelit/hardware.html#bme680-sensor-upgrade-temperatur-luftfeuchte-luftdruck-gas) support

### 0.3.14 (2021-11-14)

-   (o0shojo0o) added delay for AM2320 sesnor
-   (o0shojo0o) `Pixel Gallery` is now a part of the WebUi

### 0.3.13 (2021-10-31)

-   (o0shojo0o) added option for auto fallback animation
-   (o0shojo0o) fixed auto brightness overrides sleep mode
-   (o0shojo0o) fixed clock auto fallback overrides sleep mode

### 0.3.12 (2021-10-16)

-   (o0shojo0o) added option for DayLightSaving
-   (o0shojo0o) added option for 12H / 24H

### 0.3.11 (2021-10-14)

-   (o0shojo0o) added new WebUi on vue base

### 0.3.10 (2021-09-14)

-   (o0shojo0o) fixed clock draw

### 0.3.9 (2021-09-02)

-   (o0shojo0o) fixed auto brightness, to high values

### 0.3.8 (2021-08-11)

-   (o0shojo0o) fixed [bitmapAnimation -> rubberbanding](https://docs.bastelbunker.de/pixelit/api.html#bitmap-animation)

### 0.3.7 (2021-07-05)

-   (foorschtbar) added some UTF8 icons to [font](https://docs.bastelbunker.de/pixelit/api.html#text)
-   (foorschtbar) rewirtten UTF8 mapper function
-   (o0shojo0o) added offset options for sensors
-   (o0shojo0o) added `Clock auto fallback` function
-   (o0shojo0o) added config for `Clock auto fallback` function

### 0.3.6 (2021-06-14)

-   (o0shojo0o) added API endpoint `/api/brightness`
-   (o0shojo0o) added config for temperature (°C or °F)
-   (foorschtbar) added support for BME280
-   (foorschtbar) moved most libs to common section

### 0.3.5 (2021-05-26)

-   (o0shojo0o) extension of the API with the call setGpio
-   (o0shojo0o) added download statistics to the update page

### 0.3.4 (2021-05-20)

-   (foorschtbar) improved MQTT reconnect
-   (o0shojo0o) support decimal number input for UTC
-   (o0shojo0o) bugfix for Firefox
-   (foorschtbar) added a customizable hostname
-   (o0shojo0o) add update notification to dashboard
-   (o0shojo0o) add GitHub link to dashboard
-   (o0shojo0o) fix links in dashboard
-   (o0shojo0o) transferred the other stuff from cdn to repo
-   (foorschtbar) tweaked dashboard a little bit
-   (foorschtbar) added a note field
-   (foorschtbar) allow a Hostname as MQTT Server (Broker)
-   (o0shojo0o) add dashboard.css and pixel.js to repo
-   (foorschtbar) switches now have the proper mouse pointer

### 0.3.3 (2021-05-18)

-   (o0shojo0o) add auto brightness options
-   (o0shojo0o) add auto brightness

### 0.3.2_beta (2021-05-18)

-   (o0shojo0o) add hexColor to [Clock](https://docs.bastelbunker.de/pixelit/api.html#clock), [Text](https://docs.bastelbunker.de/pixelit/api.html#text), [Bar](https://docs.bastelbunker.de/pixelit/api.html#bar) and [Bars](https://docs.bastelbunker.de/pixelit/api.html#bars)
-   (o0shojo0o) fix wemos_d1_mini32 build
-   [(xarnze)](https://github.com/xarnze/PixelIt/commit/a8f637930d6fac131c5ce175234aff0eca6b395e) show message on hotspot mode
-   [(xarnze)](https://github.com/xarnze/PixelIt/commit/f6314351b0000c701c2243ce62895b37ff89afc2) added support for 4x 8x8 CJMCU 64 Matrix (Type 3)
-   (o0shojo0o) reactivate for esp8266 builds getSketchSize
-   (o0shojo0o) set mqtt setBufferSize to 8000
-   (o0shojo0o) add env for d1_mini
-   (rliegmann) add some missing llibs
-   (rliegmann) add some libs into .pio
-   (rliegmann) Merge pull request #2 from rliegmann/feature/clockDefaultsToFS-
-   (rliegmann) block out getSketchSize temporary
-   (rliegmann) change some build options in pio
-   (rliegmann) clock default settings, adjust MQTT buffer size, some typo
-   (rliegmann) Merge pull request #1 from rliegmann/feature/cross-esp-platform
-   (rliegmann) add ESP32 and ESP8266 libs
-   (rliegmann) remove VisualStudio overhead + port to platformIO

### 0.3.1 (2021-04-29)

-   (o0shojo0o) change MQTT_MAX_PACKET_SIZE from 4000 to 8000
-   (o0shojo0o) new versioning

### 1912182240 (2019-12-20)

-   (o0shojo0o) Added support for DFPlayer Mini MP3 Player

### 1912092038 (2019-12-08)

-   (o0shojo0o) Bugfix of the JSON from the API response
-   (o0shojo0o) Bugfix memory leak
-   (o0shojo0o) Added special characters to font

### 1910272052 (2019-10-27)

-   (o0shojo0o) Support for DHT22 added

### 1910221857 (2019-10-22)

-   (o0shojo0o) Bugfix of the JSON decoder logic (thanks to @pixelthis )

### 1909242249 (2019-09-25)

-   (o0shojo0o) There was a change in the send interval (via MQTT and Websocket) of the light sensor and the MatrixInfo
    -   The light sensor sends a maximum of once per second when the lux value changes.
    -   The MatrixInfo transmits once every 3 seconds when a value is changed.
-   (o0shojo0o) Now the protocol websocket on port 81 has been added.
-   (o0shojo0o) And the biggest innovation is the webinterface which is now available on PixelIt.

### 196232032 (2019-06-23)

-   (o0shojo0o) Added Wifimanager configuration timeout (180 seconds)

### 195161726 (2019-05-16)

-   (o0shojo0o) Added option to disable the boot screen

### 194241742 (2019-04-24)

-   (o0shojo0o) Add more option to set Matrix Temp Correction

### 19422209 (2019-04-23)

-   (o0shojo0o) Add new option to set Matrix Temp Correction

### 1942197 (2019-04-02)

-   (o0shojo0o) Bugfix animation handling

### 193312043 (2019-03-31)

-   (o0shojo0o) Bugfix DSToffset

### 193272138 (2019-03-27)

-   (o0shojo0o) Bugfix NTP TimeZone will now be interpreted correctly
-   (o0shojo0o) Add new option to set NTP-Server

### 19324816 (2019-03-24)

-   (o0shojo0o) Bugfix bitmap handling on scrolltext

### 193231249 (2019-03-23)

-   (o0shojo0o) Bugfix Erroneous representation of full screen bitmap

### 193152351 (2019-03-16)

-   (o0shojo0o) Bugfix bitmap flickering on scrolling text

### 193151540 (2019-03-15)

-   (o0shojo0o) Bugfix, there were problems with the animations in combination with the fade transition effect, so they were not displayed correctly.

### 193111919 (2019-03-11)

-   (o0shojo0o) Bugfix Clock Color: If red was set to 0, the color was always ignored.

### 1938019 (2019-03-08)

-   (o0shojo0o) add optional BitmapAnimation param: limitLoops

### 19362326 (2019-03-06)

-   (o0shojo0o) Bugfix BitmapAnimation: First frame was displayed too short.

### 19362115 (2019-03-06)

-   (o0shojo0o) Bugfix BitmapAnimation: Animation was displayed a bit too late when a text was submitted with

### 19352221 (2019-03-05)

-   (o0shojo0o) add optional Internal Clock param: withSeconds
-   (o0shojo0o) add config param: scrollTextDefaultDelay
-   (o0shojo0o) add optional Text param: scrollTextDelay
-   (o0shojo0o) add optional BitmapAnimation param: rubberbanding

### 19342040 (2019-03-04)

-   (o0shojo0o) replace umlauts
-   (o0shojo0o) add support for "Pixel Bitmap Creator" Live Preview (beta)

### 19331810 (2019-03-03)

-   (o0shojo0o) Bugfixes MQTT was always enabled
-   (o0shojo0o) add MQTT user & password
-   (o0shojo0o) add "Text-Color" for internal Clock
-   (o0shojo0o) add autoreboot after save config

### 19331112 (2019-03-03)

-   (o0shojo0o) add option to set Matrix Type at runtime (need reboot)

### 192272344 (2019-02-27)

-   (o0shojo0o) add MQTT Support
-   (o0shojo0o) add animated 8x8 icons (max. 6 frames)

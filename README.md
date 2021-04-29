![](https://raw.githubusercontent.com/o0shojo0o/ioBroker.pixelit/master/admin/pixelit.png)

# PixelIt

The Pixel It is an ESP8266 and WS2812B-LED based PixelArt display that is controlled and fed by a JSON API.
At the moment there is also a Node-Red Node ([node-red-contrib-pixelit](https://flows.nodered.org/node/node-red-contrib-pixelit)) available to the JSON API

[Wiki](https://wiki.dietru.de/books/pixel-it) |
[Forum](https://forum.bastelbunker.de/t/pixel-it) |
[Blog](https://www.bastelbunker.de/pixel-it/)

## Changelog

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

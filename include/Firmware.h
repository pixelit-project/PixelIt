#pragma once

#include <Arduino.h>
#include <memory>
#include <new>

#if defined(ESP8266)
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WiFi.h>
#include <LittleFS.h>

#elif defined(ESP32)
#include <WebServer.h>
#include <HTTPUpdateServer.h>
#include <WiFi.h>
#include <FS.h>
#endif

// BME Sensor
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include <Adafruit_BMP280.h>
#include <Adafruit_BME680.h>
// WiFi & Web
#include "PixelItWebSockets.h"
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <WiFiManager.h>
#include <Ticker.h>
// MQTT
#include <PubSubClient.h>
// Matrix
#include <Wire.h>
#include <Adafruit_GFX.h>
#if defined(ESP8266)
#define FASTLED_DEBUG_COUNT_FRAME_RETRIES
extern uint32_t _frame_cnt;
extern uint32_t _retry_cnt;
#endif
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>
// Misc
#include <BH1750.h>
#include <Max44009.h>
#include <LightDependentResistor.h>
#include <DHTesp.h>
#include <DFPlayerMini_Fast.h>
#include <SoftwareSerial.h>
#include <TimeLib.h>
#include <ArduinoJson.h>
#include <ArduinoHttpClient.h>
#include <Hash.h>
// Ulanzi Sensor
#include "Adafruit_SHT31.h"
// PixelIT Stuff
#include "PixelItFont.h"
#include "Liveview.h"
#include "AnimationLimits.h"
#include "BtnActions.h"
#include "BtnStates.h"
#include "TempSensor.h"
#include "TemperatureUnit.h"
#include "LuxSensor.h"
#include "Version.h"

// Internal Config
#define CHECKUPDATE_INTERVAL 1000UL * 60 * 60 * 8  // 8 Hours
#define CHECKUPDATESCREEN_INTERVAL 1000 * 60 * 30   // 30 Minutes
#define CHECKUPDATESCREEN_DURATION 1000 * 5         // 5 Seconds
#define CONTROL_BRIGHTNESS_INTERVAL 1000            // 1000 Milliseconds
#define SEND_TELEMETRY_INTERVAL 1000 * 60 * 60 * 12 // 12 Hours
#define SEND_LIVEVIEW_INTERVAL 250                  // 0.5 Seconds, 0 to disable
#define SEND_LUX_INTERVAL 1000 * 10                 // 10 Seconds
#define SEND_MATRIXINFO_INTERVAL 1000 * 10          // 10 Seconds
#define SEND_SENSOR_INTERVAL 1000 * 3               // 10 Seconds
#define UPDATE_BATTERY_LEVEL_INTERVAL 1000 * 30     // 30 Seconds

// Version config - will be replaced by build piple with Git-Tag!
#define VERSION "0.0.0-beta" // will be replaced by build piple with Git-Tag!

// Workaround for String in defines
#define XSTR(x) #x
#define STR(x) XSTR(x)


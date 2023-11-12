#include <Arduino.h>

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
#include <WebSocketsServer.h>
#include <WiFiClient.h>
#include <WiFiUdp.h>
#include <WiFiManager.h>
// MQTT
#include <PubSubClient.h>
// Matrix
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <FastLED.h>
#include <FastLED_NeoMatrix.h>
// Misc
#include <BH1750.h>
#include <Max44009.h>
#include <LightDependentResistor.h>
#include <DHTesp.h>
#include <DFPlayerMini_Fast.h>
#include <SoftwareSerial.h>
#include "ColorConverterLib.h"
#include <TimeLib.h>
#include <ArduinoJson.h>
#include <ArduinoHttpClient.h>
#include <Hash.h>
// Ulanzi Sensor
#include "Adafruit_SHT31.h"
// PixelIT Stuff
#include "PixelItFont.h"
#include "Webinterface.h"
#include "Tools.h"
#include "UpdateScreen.h"
#include "Liveview.h"

// Internal Config
#define CHECKUPDATE_INTERVAL 1000 * 60 * 6 * 8      // 8 Hours
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

void FadeOut(int = 10, int = 0);
void FadeIn(int = 10, int = 0);

class SetGPIO
{
public:
    int gpio;
    ulong resetMillis;
};
#define SET_GPIO_SIZE 4
SetGPIO setGPIOReset[SET_GPIO_SIZE];

//// MQTT Config
bool mqttAktiv = false;
String mqttUser = "";
String mqttPassword = "";
String mqttServer = "";
String mqttMasterTopic = "pixelit/";
String mqttDeviceTopic = "";
bool mqttUseDeviceTopic = true;
bool mqttHAdiscoverable = true;
int mqttPort = 1883;
unsigned long mqttLastReconnectAttempt = 0; // will store last time reconnect to mqtt broker
const int MQTT_RECONNECT_INTERVAL = 15000;
// #define MQTT_MAX_PACKET_SIZE 8000

String dfpRXPin = STR(DEFAULT_PIN_DFPRX);
String dfpTXPin = STR(DEFAULT_PIN_DFPTX);
String onewirePin = STR(DEFAULT_PIN_ONEWIRE);
String SCLPin = STR(DEFAULT_PIN_SCL);
String SDAPin = STR(DEFAULT_PIN_SDA);
String ldrDevice = STR(DEFAULT_LDR);
unsigned long ldrPulldown = 10000; // 10k pulldown-resistor
unsigned int ldrSmoothing = 0;

// Battery stuff
float batteryLevel = 0;
unsigned long batteryLevelPrevMillis = 0;

#ifndef MIN_BATTERY
#define MIN_BATTERY 0
#endif

#ifndef MAX_BATTERY
#define MAX_BATTERY 100
#endif

// Telemetry API
#define TELEMETRY_SERVER_HOST "pixelit.bastelbunker.de"
#define TELEMETRY_SERVER_PATH "/api/telemetry"
#define TELEMETRY_SERVER_PORT 80

// Check Update API
#define CHECKUPDATE_SERVER_HOST "pixelit.bastelbunker.de"
#define CHECKUPDATE_SERVER_PATH "/api/lastversion"
#define CHECKUPDATE_SERVER_PORT 80

int btnPressedLevel[] = {LOW, LOW, LOW};

enum btnStates
{
    btnState_Released,
    btnState_PressedNew,
    btnState_PressedBefore,
};

const String btnAPINames[]{"leftButton", "middleButton", "rightButton"};
const String btnLogNames[]{"Left button", "Middle button", "Right button"};

btnStates btnState[] = {btnState_Released, btnState_Released, btnState_Released};
bool btnLastPublishState[] = {false, false, false};

enum btnActions
{
    btnAction_DoNothing = 0,
    btnAction_GotoClock = 1,
    btnAction_ToggleSleepMode = 2,
    btnAction_MP3PlayPause = 3,
    btnAction_MP3PlayPrevious = 4,
    btnAction_MP3PlayNext = 5,
};

#if defined(ULANZI)
String btnPin[] = {"GPIO_NUM_26", "GPIO_NUM_27", "GPIO_NUM_14"}; // UlanziTC001 workaround to tweak WebUI
bool btnEnabled[] = {true, true, true};
btnActions btnAction[] = {btnAction_DoNothing, btnAction_ToggleSleepMode, btnAction_GotoClock};
#else
String btnPin[] = {"Pin_D0", "Pin_D4", "Pin_D5"};
bool btnEnabled[] = {false, false, false};
btnActions btnAction[] = {btnAction_ToggleSleepMode, btnAction_GotoClock, btnAction_DoNothing};
#endif

CRGB leds[MATRIX_WIDTH * MATRIX_HEIGHT];

#if defined(ESP8266)
bool isESP8266 = true;
#else
bool isESP8266 = false;
#endif

#if defined(ESP32)
TwoWire twowire(BME280_ADDRESS_ALTERNATE);
#elif defined(ULANZI)
TwoWire twowire = TwoWire(0);
#else
TwoWire twowire;
#endif
Adafruit_BME280 *bme280;
Adafruit_BMP280 *bmp280;
Adafruit_BME680 *bme680;
Adafruit_SHT31 sht31 = Adafruit_SHT31(&twowire);
unsigned long lastBME680read = 0;
DHTesp dht;

// TempSensor
enum TempSensor
{
    TempSensor_None,
    TempSensor_BME280,
    TempSensor_DHT,
    TempSensor_BME680,
    TempSensor_BMP280,
    TempSensor_SHT31,
};
TempSensor tempSensor = TempSensor_None;

// TemperatureUnit
enum TemperatureUnit
{
    TemperatureUnit_Celsius,
    TemperatureUnit_Fahrenheit
};
TemperatureUnit temperatureUnit = TemperatureUnit_Celsius;

LightDependentResistor *photocell;
BH1750 *bh1750;
Max44009 *max44009;

enum LuxSensor
{
    LuxSensor_LDR,
    LuxSensor_BH1750,
    LuxSensor_Max44009,
};
LuxSensor luxSensor = LuxSensor_LDR;

FastLED_NeoMatrix *matrix;
WiFiClient wifiClientMQTT;
WiFiClient wifiClientHTTP;
WiFiUDP udp;
PubSubClient client(wifiClientMQTT);
WiFiManager wifiManager;
#if defined(ESP8266)
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
#elif defined(ESP32)
WebServer server(80);
HTTPUpdateServer httpUpdater;
#endif
Liveview liveview;
// Store last frame (serializated)
String currentScreenJsonBuffer;

WebSocketsServer webSocket = WebSocketsServer(81);
DFPlayerMini_Fast mp3Player;
SoftwareSerial *softSerial;
uint initialVolume = 10;

// Matrix Vars
int currentMatrixBrightness = 127;
bool matrixBrightnessAutomatic = true;
int mbaDimMin = 20;
int mbaDimMax = 100;
int mbaLuxMin = 0;
int mbaLuxMax = 400;
int matrixType = DEFAULT_MATRIX_TYPE;
String note;
String hostname;
String deviceID;
String matrixTempCorrection = "default";

// System Vars
bool sleepMode = false;
bool bootScreenAktiv = true;
bool bootBatteryScreen = VBAT_PIN > 0 ? true : false;
bool bootSound = false;
String optionsVersion = "";
// Millis timestamp of the last receiving screen
unsigned long lastScreenMessageMillis = 0;
unsigned long lastGetBatteryPercent = 0;

// Bmp Vars
uint16_t bmpArray[64];
bool withBMP = false;
int bmpWidth = 8;
int bmpHeight = 8;
int bmpPosX = 0;
int bmpPosY = 0;

// Timerserver Vars
String ntpServer = "de.pool.ntp.org";
uint ntpRetryCounter = 0;
unsigned long ntpTimeOut = 0;
#define NTP_MAX_RETRYS 3
#define NTP_TIMEOUT_SEC 60

// Clock  Vars
bool clockBlink = false;
bool clockAktiv = true;
bool clock24Hours = true;
bool clockDateDayMonth = true;
bool clockDayOfWeekFirstMonday = true;
bool clockDayLightSaving = true;
bool clockSwitchAktiv = true;
bool clockWithSeconds = false;
bool clockAutoFallbackActive = false;
uint clockAutoFallbackAnimation = 1;
uint clockSwitchSec = 7;
uint clockCounterClock = 0;
uint clockCounterDate = 0;
float clockTimeZone = 1;
time_t clockLastUpdate;
uint8_t clockColorR = 255, clockColorG = 255, clockColorB = 255;
uint clockAutoFallbackTime = 30;
bool forceClock = false;
bool clockBlinkAnimated = true;
bool clockLargeFont = false;
bool clockFatFont = false;
bool clockDrawWeekDays = true;

// Scrolltext Vars
bool scrollTextAktivLoop = false;
unsigned long scrollTextPrevMillis = 0;
uint scrollTextDefaultDelay = 100;
uint scrollTextDelay;
int scrollCurPos;
int scrollposY;
int scrollposX;
int scrollxTextWidth;
int scrollxAvailableTextSpace;
String scrollTextString;

// Animate BMP Vars
uint16_t animationBmpList[10][64];
bool animateBMPAktivLoop = false;
unsigned long animateBMPPrevMillis = 0;
int animateBMPCounter = 0;
bool animateBMPReverse = false;
bool animateBMPRubberbandingAktiv = false;
uint animateBMPDelay;
int animateBMPLimitLoops = -1;
int animateBMPLoopCount = 0;
int animateBMPLimitFrames = -1;
int animateBMPFrameCount = 0;

// Sensors Vars
unsigned long sendLuxPrevMillis = 0;
unsigned long getLuxPrevMillis = 0;
unsigned long sendSensorPrevMillis = 0;
unsigned long sendInfoPrevMillis = 0;
String oldGetLuxSensor;
String oldGetSensor;
float currentLux = 0.0f;
float luxOffset = 0.0f;
float temperatureOffset = 0.0f;
float humidityOffset = 0.0f;
float pressureOffset = 0.0f;
float gasOffset = 0.0f;

// Other Vars
bool sendTelemetry = true;
unsigned long sendTelemetryPrevMillis = 0;
unsigned long forcedScreenIsActiveUntil = 0;
bool checkUpdateScreen = true;
unsigned long checkUpdateScreenPrevMillis = 0;
unsigned long checkUpdatePrevMillis = 0;
String lastReleaseVersion = VERSION;

typedef struct
{
    int major;
    int minor;
    int patch;
    char prerelease[16];
} Version;

// MP3Player Vars
String OldGetMP3PlayerInfo;

// Websoket Vars
String websocketConnection[10];

String ResetReason()
{
#if defined(ESP8266)
    return ESP.getResetReason();
#elif defined(ESP32)
    switch (esp_reset_reason())
    {
    case ESP_RST_POWERON:
        return "Power-on reset";
    case ESP_RST_EXT:
        return "External reset";
    case ESP_RST_SW:
        return "Software reset";
    case ESP_RST_PANIC:
        return "Panic (hardware or software)";
    case ESP_RST_INT_WDT:
        return "Internal watchdog reset";
    case ESP_RST_TASK_WDT:
        return "Task watchdog reset";
    case ESP_RST_WDT:
        return "Watchdog reset";
    case ESP_RST_DEEPSLEEP:
        return "Deep sleep reset";
    case ESP_RST_BROWNOUT:
        return "Brownout reset";
    case ESP_RST_SDIO:
        return "SDIO reset";
    default:
        return "Unknown reset reason";
    }
#endif
}

Version parseVersion(const char *versionStr)
{
    Version version;

    memset(version.prerelease, 0, sizeof(version.prerelease));

    sscanf(versionStr, "%d.%d.%d-%s", &version.major, &version.minor, &version.patch, version.prerelease);
    return version;
}

int compareVersions(const char *version1, const char *version2)
{
    if (strlen(version1) == 0 || strlen(version2) == 0)
    {
        return 0;
    }

    Version v1 = parseVersion(version1);
    Version v2 = parseVersion(version2);

    if (v1.major != v2.major)
    {
        return v1.major - v2.major;
    }
    if (v1.minor != v2.minor)
    {
        return v1.minor - v2.minor;
    }
    if (v1.patch != v2.patch)
    {
        return v1.patch - v2.patch;
    }

    if (strlen(v1.prerelease) == 0 && strlen(v2.prerelease) == 0)
    {
        return 0; // Versions are equal
    }
    else if (strlen(v1.prerelease) == 0)
    {
        return 1; // v1 is greater (no prerelease for v1, but prerelease for v2)
    }
    else if (strlen(v2.prerelease) == 0)
    {
        return -1; // v2 is greater (no prerelease for v2, but prerelease for v1)
    }
    else
    {
        return strcmp(v1.prerelease, v2.prerelease); // Compare prerelease strings
    }
}

void getBatteryVoltage()
{
    uint16_t value = 0;
    uint8_t numReadings = 5;

    for (uint8_t i = 0; i < numReadings; i++)
    {
        value = value + analogRead(VBAT_PIN);

        // 1ms pause adds more stability between reads.
        delay(1);
    }

    batteryLevel = map(value / numReadings, MIN_BATTERY, MAX_BATTERY, 0, 100);
    if (batteryLevel >= 100)
    {
        batteryLevel = 100;
    }
    if (batteryLevel <= 0)
    {
        batteryLevel = 1;
    }
}

void SetCurrentMatrixBrightness(int newBrightness)
{
    currentMatrixBrightness = newBrightness;
    matrix->setBrightness(currentMatrixBrightness);
}

void EnteredHotspotCallback(WiFiManager *manager)
{
    Log(F("Hotspot"), "Waiting for WiFi configuration");
    matrix->clear();
    DrawTextHelper("HOTSPOT", false, false, false, false, false, 255, 255, 255, 3, 1);
    FadeIn();
}

void SaveConfig()
{
    // save the custom parameters to FS
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.createObject();

    json["version"] = VERSION;
    json["isESP8266"] = isESP8266;
    json["temperatureUnit"] = static_cast<int>(temperatureUnit);
    json["matrixBrightnessAutomatic"] = matrixBrightnessAutomatic;
    json["mbaDimMin"] = mbaDimMin;
    json["mbaDimMax"] = mbaDimMax;
    json["mbaLuxMin"] = mbaLuxMin;
    json["mbaLuxMax"] = mbaLuxMax;
    json["matrixBrightness"] = currentMatrixBrightness;
    json["matrixType"] = matrixType;
    json["note"] = note;
    json["hostname"] = hostname;
    json["matrixTempCorrection"] = matrixTempCorrection;
    json["ntpServer"] = ntpServer;
    json["clockTimeZone"] = clockTimeZone;

    String clockColorHex;
    ColorConverter::RgbToHex(clockColorR, clockColorG, clockColorB, clockColorHex);
    json["clockColor"] = "#" + clockColorHex;

    json["clockSwitchAktiv"] = clockSwitchAktiv;
    json["clockSwitchSec"] = clockSwitchSec;
    json["clock24Hours"] = clock24Hours;
    json["clockDayLightSaving"] = clockDayLightSaving;
    json["clockWithSeconds"] = clockWithSeconds;
    json["clockAutoFallbackActive"] = clockAutoFallbackActive;
    json["clockAutoFallbackTime"] = clockAutoFallbackTime;
    json["clockAutoFallbackAnimation"] = clockAutoFallbackAnimation;
    json["clockDateDayMonth"] = clockDateDayMonth;
    json["clockDayOfWeekFirstMonday"] = clockDayOfWeekFirstMonday;
    json["clockBlinkAnimated"] = clockBlinkAnimated;
    json["clockLargeFont"] = clockLargeFont;
    json["clockFatFont"] = clockFatFont;
    json["clockDrawWeekDays"] = clockDrawWeekDays;
    json["scrollTextDefaultDelay"] = scrollTextDefaultDelay;
    json["bootScreenAktiv"] = bootScreenAktiv;
    json["bootBatteryScreen"] = bootBatteryScreen;
    json["bootSound"] = bootSound;
    json["mqttAktiv"] = mqttAktiv;
    json["mqttUser"] = mqttUser;
    json["mqttPassword"] = mqttPassword;
    json["mqttServer"] = mqttServer;
    json["mqttMasterTopic"] = mqttMasterTopic;
    json["mqttPort"] = mqttPort;
    json["mqttUseDeviceTopic"] = mqttUseDeviceTopic;
    json["mqttHAdiscoverable"] = mqttHAdiscoverable;
    json["luxOffset"] = luxOffset;
    json["temperatureOffset"] = temperatureOffset;
    json["humidityOffset"] = humidityOffset;
    json["pressureOffset"] = pressureOffset;
    json["gasOffset"] = gasOffset;

    json["dfpRXpin"] = dfpRXPin;
    json["dfpTXpin"] = dfpTXPin;
    json["onewirePin"] = onewirePin;
    json["SCLPin"] = SCLPin;
    json["SDAPin"] = SDAPin;
    for (uint b = 0; b < 3; b++)
    {
        json["btn" + String(b) + "Pin"] = btnPin[b];
        json["btn" + String(b) + "PressedLevel"] = btnPressedLevel[b];
        json["btn" + String(b) + "Enabled"] = btnEnabled[b];
        json["btn" + String(b) + "Action"] = static_cast<int>(btnAction[b]);
    }
    json["ldrDevice"] = ldrDevice;
    json["ldrPulldown"] = ldrPulldown;
    json["ldrSmoothing"] = ldrSmoothing;
    json["initialVolume"] = initialVolume;
    json["sendTelemetry"] = sendTelemetry;
    json["checkUpdateScreen"] = checkUpdateScreen;

#if defined(ESP8266)
    File configFile = LittleFS.open("/config.json", "w");
#elif defined(ESP32)
    File configFile = SPIFFS.open("/config.json", "w");
#endif
    json.printTo(configFile);
    configFile.close();
    Log("SaveConfig", "Saved");
    // end save
}

void LoadConfig()
{
    // file exists, reading and loading
#if defined(ESP8266)
    if (LittleFS.exists("/config.json"))
    {
        File configFile = LittleFS.open("/config.json", "r");
#elif defined(ESP32)
    if (SPIFFS.exists("/config.json"))
    {
        File configFile = SPIFFS.open("/config.json", "r");
#endif
        if (configFile)
        {
            // Serial.println("opened config file");

            DynamicJsonBuffer jsonBuffer;
            JsonObject &json = jsonBuffer.parseObject(configFile);

            if (json.success())
            {
                SetConfigVariables(json);
                Log("LoadConfig", "Loaded");
            }
        }
    }
    else
    {
        Log("LoadConfig", "No Configfile, init new file");
        SaveConfig();
    }
}

void SetConfig(JsonObject &json)
{
    SetConfigVariables(json);
    SaveConfig();
}

void SetConfigVariables(JsonObject &json)
{
    if (json.containsKey("version"))
    {
        optionsVersion = json["version"].as<String>();
    }

    if (json.containsKey("temperatureUnit"))
    {
        temperatureUnit = static_cast<TemperatureUnit>(json["temperatureUnit"].as<int>());
    }

    if (json.containsKey("matrixBrightnessAutomatic"))
    {
        matrixBrightnessAutomatic = json["matrixBrightnessAutomatic"].as<bool>();
    }

    if (json.containsKey("mbaDimMin"))
    {
        mbaDimMin = json["mbaDimMin"].as<int>();
    }

    if (json.containsKey("mbaDimMax"))
    {
        mbaDimMax = json["mbaDimMax"].as<int>();
    }

    if (json.containsKey("mbaLuxMin"))
    {
        mbaLuxMin = json["mbaLuxMin"].as<int>();
    }

    if (json.containsKey("mbaLuxMax"))
    {
        mbaLuxMax = json["mbaLuxMax"].as<int>();
    }

    if (json.containsKey("matrixBrightness"))
    {
        SetCurrentMatrixBrightness(json["matrixBrightness"].as<int>());
    }

    if (json.containsKey("matrixType"))
    {
        matrixType = json["matrixType"].as<int>();
    }

    if (json.containsKey("note"))
    {
        note = json["note"].as<char *>();
    }

    if (json.containsKey("hostname"))
    {
        String hostname_raw = json["hostname"].as<char *>();
        hostname = "";
        for (uint8_t n = 0; n < hostname_raw.length(); n++)
        {
            if ((hostname_raw.charAt(n) >= '0' && hostname_raw.charAt(n) <= '9') || (hostname_raw.charAt(n) >= 'A' && hostname_raw.charAt(n) <= 'Z') || (hostname_raw.charAt(n) >= 'a' && hostname_raw.charAt(n) <= 'z') || (hostname_raw.charAt(n) == '_') || (hostname_raw.charAt(n) == '-'))
                hostname += hostname_raw.charAt(n);
        }
    }

    if (json.containsKey("matrixTempCorrection"))
    {
        matrixTempCorrection = json["matrixTempCorrection"].as<char *>();
    }

    if (json.containsKey("ntpServer"))
    {
        ntpServer = json["ntpServer"].as<char *>();
    }

    if (json.containsKey("clockTimeZone"))
    {
        clockTimeZone = json["clockTimeZone"].as<float>();
    }

    if (json.containsKey("clockColor"))
    {
        ColorConverter::HexToRgb(json["clockColor"].as<String>(), clockColorR, clockColorG, clockColorB);
    }

    if (json.containsKey("clockSwitchAktiv"))
    {
        clockSwitchAktiv = json["clockSwitchAktiv"].as<bool>();
    }

    if (json.containsKey("clockSwitchSec"))
    {
        clockSwitchSec = json["clockSwitchSec"].as<uint>();
    }

    if (json.containsKey("clock24Hours"))
    {
        clock24Hours = json["clock24Hours"].as<bool>();
    }

    if (json.containsKey("clockDayLightSaving"))
    {
        clockDayLightSaving = json["clockDayLightSaving"].as<bool>();
    }

    if (json.containsKey("clockWithSeconds"))
    {
        clockWithSeconds = json["clockWithSeconds"].as<bool>();
    }

    if (json.containsKey("clockBlinkAnimated"))
    {
        clockBlinkAnimated = json["clockBlinkAnimated"].as<bool>();
    }

    if (json.containsKey("clockAutoFallbackActive"))
    {
        clockAutoFallbackActive = json["clockAutoFallbackActive"].as<bool>();
    }

    if (json.containsKey("clockAutoFallbackAnimation"))
    {
        clockAutoFallbackAnimation = json["clockAutoFallbackAnimation"].as<uint>();
    }

    if (json.containsKey("clockAutoFallbackTime"))
    {
        clockAutoFallbackTime = json["clockAutoFallbackTime"].as<uint>();
    }

    if (json.containsKey("clockDateDayMonth"))
    {
        clockDateDayMonth = json["clockDateDayMonth"].as<bool>();
    }

    if (json.containsKey("clockDayOfWeekFirstMonday"))
    {
        clockDayOfWeekFirstMonday = json["clockDayOfWeekFirstMonday"].as<bool>();
    }

    if (json.containsKey("clockLargeFont"))
    {
        clockLargeFont = json["clockLargeFont"].as<bool>();
    }

    if (json.containsKey("clockFatFont"))
    {
        clockFatFont = json["clockFatFont"].as<bool>();
    }
    if (json.containsKey("clockDrawWeekDays"))
    {
        clockDrawWeekDays = json["clockDrawWeekDays"].as<bool>();
    }

    if (json.containsKey("scrollTextDefaultDelay"))
    {
        scrollTextDefaultDelay = json["scrollTextDefaultDelay"].as<uint>();
    }

    if (json.containsKey("bootScreenAktiv"))
    {
        bootScreenAktiv = json["bootScreenAktiv"].as<bool>();
    }

    if (json.containsKey("bootBatteryScreen"))
    {
        bootBatteryScreen = json["bootBatteryScreen"].as<bool>();
    }

    if (json.containsKey("bootSound"))
    {
        bootSound = json["bootSound"].as<bool>();
    }

    if (json.containsKey("mqttAktiv"))
    {
        mqttAktiv = json["mqttAktiv"].as<bool>();
    }

    if (json.containsKey("mqttUser"))
    {
        mqttUser = json["mqttUser"].as<char *>();
    }

    if (json.containsKey("mqttPassword"))
    {
        mqttPassword = json["mqttPassword"].as<char *>();
    }

    if (json.containsKey("mqttServer"))
    {
        mqttServer = json["mqttServer"].as<char *>();
    }

    if (json.containsKey("mqttMasterTopic"))
    {
        mqttMasterTopic = json["mqttMasterTopic"].as<char *>();
        mqttMasterTopic.trim();
        if (!mqttMasterTopic.endsWith("/"))
        {
            mqttMasterTopic += "/";
        }
    }

    if (json.containsKey("mqttPort"))
    {
        mqttPort = json["mqttPort"].as<int>();
    }

    if (json.containsKey("mqttUseDeviceTopic"))
    {
        mqttUseDeviceTopic = json["mqttUseDeviceTopic"].as<bool>();
    }

    if (json.containsKey("mqttHAdiscoverable"))
    {
        mqttHAdiscoverable = json["mqttHAdiscoverable"].as<bool>();
    }

    if (json.containsKey("luxOffset"))
    {
        luxOffset = json["luxOffset"].as<float>();
    }

    if (json.containsKey("temperatureOffset"))
    {
        temperatureOffset = json["temperatureOffset"].as<float>();
    }

    if (json.containsKey("humidityOffset"))
    {
        humidityOffset = json["humidityOffset"].as<float>();
    }

    if (json.containsKey("pressureOffset"))
    {
        pressureOffset = json["pressureOffset"].as<float>();
    }

    if (json.containsKey("gasOffset"))
    {
        gasOffset = json["gasOffset"].as<float>();
    }

    if (json.containsKey("dfpRXpin"))
    {
        dfpRXPin = json["dfpRXpin"].as<char *>();
    }

    if (json.containsKey("dfpTXpin"))
    {
        dfpTXPin = json["dfpTXpin"].as<char *>();
    }

    if (json.containsKey("onewirePin"))
    {
        onewirePin = json["onewirePin"].as<char *>();
    }

    if (json.containsKey("SCLPin"))
    {
        SCLPin = json["SCLPin"].as<char *>();
    }

    if (json.containsKey("SDAPin"))
    {
        SDAPin = json["SDAPin"].as<char *>();
    }

    for (uint b = 0; b < 3; b++)
    {
        if (json.containsKey("btn" + String(b) + "Pin"))
        {
            btnPin[b] = json["btn" + String(b) + "Pin"].as<char *>();
        }
        if (json.containsKey("btn" + String(b) + "PressedLevel"))
        {
            btnPressedLevel[b] = json["btn" + String(b) + "PressedLevel"].as<int>();
        }
        if (json.containsKey("btn" + String(b) + "Enabled"))
        {
            btnEnabled[b] = json["btn" + String(b) + "Enabled"].as<bool>();
        }
        if (json.containsKey("btn" + String(b) + "Action"))
        {
            btnAction[b] = static_cast<btnActions>(json["btn" + String(b) + "Action"].as<int>());
        }
    }

    if (json.containsKey("ldrDevice"))
    {
        ldrDevice = json["ldrDevice"].as<char *>();
    }

    if (json.containsKey("ldrPulldown"))
    {
        ldrPulldown = json["ldrPulldown"].as<unsigned long>();
    }

    if (json.containsKey("ldrSmoothing"))
    {
        ldrSmoothing = json["ldrSmoothing"].as<uint>();
    }

    if (json.containsKey("initialVolume"))
    {
        initialVolume = json["initialVolume"].as<uint>();
    }

    if (json.containsKey("sendTelemetry"))
    {
        sendTelemetry = json["sendTelemetry"].as<bool>();
    }

    if (json.containsKey("checkUpdateScreen"))
    {
        checkUpdateScreen = json["checkUpdateScreen"].as<bool>();
    }
}

void EraseWifiCredentials()
{
    wifiManager.resetSettings();
    delay(300);
    ESP.restart();
    delay(300);
}

void HandleGetMainPage()
{
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("text/html"), mainPage);
}

void HandleNotFound()
{
    if (server.method() == HTTP_OPTIONS)
    {
        server.sendHeader(F("Access-Control-Allow-Origin"), "*");
        server.send(204);
    }

    server.sendHeader("Location", "/update", true);
    server.send(302, F("text/plain"), "");
}

void HandleScreen()
{
    DynamicJsonBuffer jsonBuffer;
    String args = String(server.arg("plain").c_str());
    JsonObject &json = jsonBuffer.parseObject(args.begin());
    server.sendHeader(F("Connection"), F("close"));
    server.sendHeader(F("Access-Control-Allow-Origin"), "*");

    if (json.success())
    {
        server.send(200, F("application/json"), F("{\"response\":\"OK\"}"));
        Log(F("HandleScreen"), "Incoming JSON length: " + String(json.measureLength()));
        CreateFrames(json);
    }
    else
    {
        server.send(406, F("application/json"), F("{\"response\":\"Not Acceptable\"}"));
    }
}

void HandleSetConfig()
{
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.parseObject(server.arg("plain"));
    server.sendHeader(F("Connection"), F("close"));

    if (json.success())
    {
        Log(F("SetConfig"), "Incoming JSON length: " + String(json.measureLength()));
        SetConfig(json);
        server.send(200, F("application/json"), F("{\"response\":\"OK\"}"));
        delay(500);
        ESP.restart();
    }
    else
    {
        server.send(406, F("application/json"), F("{\"response\":\"Not Acceptable\"}"));
    }
}

void HandleGetConfig()
{
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("application/json"), GetConfig());
}

void HandleGetLuxSensor()
{
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("application/json"), GetLuxSensor());
}

void HandleGetBrightness()
{
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("application/json"), GetBrightness());
}

void HandleGetDHTSensor() // Legancy
{
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("application/json"), GetSensor());
}

void HandleGetSensor()
{
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("application/json"), GetSensor());
}

void HandleGetButtons()
{
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("application/json"), GetButtons());
}

void HandleGetMatrixInfo()
{
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("application/json"), GetMatrixInfo());
}

void HandelWifiConfigReset()
{
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("application/json"), F("{\"response\":\"OK\"}"));
    EraseWifiCredentials();
}

void HandleFactoryReset()
{
    server.sendHeader(F("Connection"), F("close"));
    server.send(200, F("application/json"), F("{\"response\":\"OK\"}"));
#if defined(ESP8266)
    File configFile = LittleFS.open("/config.json", "w");
#elif defined(ESP32)
    File configFile = SPIFFS.open("/config.json", "w");
#endif
    if (!configFile)
    {
        Log(F("Handle_factoryreset"), F("Failed to open config file for reset"));
    }
    configFile.println("");
    configFile.close();
    EraseWifiCredentials();
}

void SleepScreen(bool startSleep, bool forceClockOnWake)
{
    if (startSleep)
    {
        Log(F("SleepScreen"), F("Sleeping..."));
        matrix->clear();
        DrawTextHelper("z", false, false, false, false, false, 0, 0, 255, (MATRIX_WIDTH / 2) - 6, 1);
        matrix->show();
        delay(200);
        DrawTextHelper("Z", false, false, false, false, false, 0, 0, 255, (MATRIX_WIDTH / 2) - 1, 1);
        matrix->show();
        delay(200);
        DrawTextHelper("z", false, false, false, false, false, 0, 0, 255, (MATRIX_WIDTH / 2) + 4, 1);
        matrix->show();
        delay(500);
        FadeOut(30, 0);
        matrix->setBrightness(0);
        matrix->show();
    }
    else
    {
        Log(F("SleepScreen"), F("Waking up..."));
        matrix->clear();
        // DrawTextHelper("😀", false, true, false, false, false, 255, 200, 0, 0, 1);
        // FadeIn(30, 0);
        // delay(150);
        forceClock = forceClockOnWake;
    }
}

void HandleAndSendButtonPress(uint button, bool state)
{
    btnLastPublishState[button] = state;
    Log(F("Buttons"), btnLogNames[button] + " is now " + (state ? "true" : "false"));

    // Prüfen ob über MQTT versendet werden muss
    if (mqttAktiv == true && client.connected())
    {
        client.publish((mqttMasterTopic + "buttons/button" + String(button)).c_str(), (state ? "true" : "false"), true);
        if (mqttUseDeviceTopic)
        {
            client.publish((mqttDeviceTopic + "buttons/button" + String(button)).c_str(), (state ? "true" : "false"), true);
        }
    }
    // Prüfen ob über Websocket versendet werden muss
    if (webSocket.connectedClients() > 0)
    {
        for (uint i = 0; i < sizeof websocketConnection / sizeof websocketConnection[0]; i++)
        {
            webSocket.sendTXT(i, "{\"buttons\":{\"" + btnAPINames[button] + "\":" + (state ? "true" : "false") + "}}");
        }
    }

    if (state == false)
    {
        return;
    }

    if (btnAction[button] == btnAction_ToggleSleepMode)
    {
        sleepMode = !sleepMode;
        SleepScreen(sleepMode, true);
    }
    if (btnAction[button] == btnAction_GotoClock)
    {
        forceClock = true;
    }
    if (btnAction[button] == btnAction_MP3PlayPrevious)
    {
        mp3Player.playPrevious();
    }
    if (btnAction[button] == btnAction_MP3PlayNext)
    {
        mp3Player.playNext();
    }
    if (btnAction[button] == btnAction_MP3PlayPause)
    {
        if (mp3Player.isPlaying())
        {
            mp3Player.pause();
        }
        else
        {
            delay(200);
            mp3Player.resume();
        }
    }
}

void callback(char *topic, byte *payload, unsigned int length)
{
    if (payload[0] == '{')
    {
        payload[length] = '\0';
        String channel = String(topic);
        int lastSlashIndex = channel.lastIndexOf('/');
        if (lastSlashIndex != -1)
        {
            channel = channel.substring(lastSlashIndex + 1);
        }

        DynamicJsonBuffer jsonBuffer;
        JsonObject &json = jsonBuffer.parseObject(payload);

        Log("MQTT_callback", "Incoming JSON (Topic: " + String(topic) + ", Cmd: " + channel + ", Bytes: " + String(length) + "/" + String(json.measureLength()) + ") ");

        if (json.measureLength() == 2)
        {
            Log("MQTT_callback", "JSON message empty or too long");
            return;
        }
        if (channel.equals("setScreen"))
        {
            CreateFrames(json);
        }
        else if (channel.equals("getLuxsensor"))
        {
            client.publish((mqttMasterTopic + "luxsensor").c_str(), GetLuxSensor().c_str());
            if (mqttUseDeviceTopic)
            {
                client.publish((mqttDeviceTopic + "luxsensor").c_str(), GetLuxSensor().c_str());
            }
        }
        else if (channel.equals("getMatrixinfo"))
        {
            client.publish((mqttMasterTopic + "matrixinfo").c_str(), GetMatrixInfo().c_str());
            if (mqttUseDeviceTopic)
            {
                client.publish((mqttDeviceTopic + "matrixinfo").c_str(), GetMatrixInfo().c_str());
            }
        }
        else if (channel.equals("getConfig"))
        {
            client.publish((mqttMasterTopic + "config").c_str(), GetConfig().c_str());
            if (mqttUseDeviceTopic)
            {
                client.publish((mqttDeviceTopic + "config").c_str(), GetConfig().c_str());
            }
        }
        else if (channel.equals("setConfig"))
        {
            SetConfig(json);
        }
    }
}

void webSocketEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length)
{
    switch (type)
    {
    case WStype_DISCONNECTED:
    {
        Log("WebSocketEvent", "[" + String(num) + "] Disconnected!");
        websocketConnection[num] = "";
        break;
    }
    case WStype_CONNECTED:
    {
        // Remember for what the connection was established
        websocketConnection[num] = String((char *)payload);

        // get ip
        IPAddress ip = webSocket.remoteIP(num);

        // Logging
        Log(F("WebSocketEvent"), "[" + String(num) + "] Connected from " + ip.toString() + " url: " + websocketConnection[num]);

        // send message to client
        SendMatrixInfo();
        SendLDR(true);
        SendSensor(true);
        SendConfig();
        webSocket.sendTXT(num, "{\"buttons\":" + GetButtons() + "}");
        webSocket.sendTXT(num, "{\"telemetry\":" + GetTelemetry() + "}");
        break;
    }
    case WStype_TEXT:
    {
        if (((char *)payload)[0] == '{')
        {
            DynamicJsonBuffer jsonBuffer;
            JsonObject &json = jsonBuffer.parseObject(payload);
            int forcedDuration = 0;

            // Logging
            Log(F("WebSocketEvent"), "Incoming JSON (Length: " + String(length) + "/" + String(json.measureLength()) + ")");
            if (length != json.measureLength())
            {
                Log("MQTT_callback", "JSON length mismatch! JSON Message to long :(");
                return;
            }

            if (json.containsKey("forcedDuration"))
            {
                forcedDuration = json["forcedDuration"].as<int>();
            }

            if (json.containsKey("setScreen"))
            {
                CreateFrames(json["setScreen"], forcedDuration);
            }
            else if (json.containsKey("setConfig"))
            {
                SetConfig(json["setConfig"]);
                delay(500);
                ESP.restart();
            }
            else if (json.containsKey("wifiReset"))
            {
                if (json["wifiReset"].as<bool>() == true)
                {
                    HandelWifiConfigReset();
                }
            }
            else if (json.containsKey("factoryReset"))
            {
                if (json["factoryReset"].as<bool>() == true)
                {
                    HandleFactoryReset();
                }
            }
            else if (json.containsKey("sendTelemetry"))
            {
                sendTelemetryPrevMillis = 0;
            }
        }
        break;
    }
    case WStype_BIN:
        break;
    case WStype_FRAGMENT_BIN_START:
        break;
    case WStype_FRAGMENT_TEXT_START:
        break;
    case WStype_FRAGMENT:
        break;
    case WStype_FRAGMENT_FIN:
        break;
    case WStype_PING:
        break;
    case WStype_PONG:
        break;
    case WStype_ERROR:
        break;
    }
}

void CreateFrames(JsonObject &json)
{
    CreateFrames(json, 0);
}

void CreateFrames(JsonObject &json, int forceDuration)
{
    bool sendMatrixInfo = false;

    String logMessage = F("JSON contains ");

    if (json.containsKey("sleepMode"))
    {

        logMessage += F("Sleep Control, ");
        Serial.printf("SleepMode: %s\n", json["sleepMode"].as<bool>() ? "true" : "false");

        // Update internal sleep state
        bool newSleepMode = json["sleepMode"].as<bool>();

        if (newSleepMode != sleepMode)
        {
            sendMatrixInfo = true;
            // Update sleep mode
            sleepMode = newSleepMode;

            // Display sleepscreen
            SleepScreen(sleepMode, false);

            // Restore last frame if sleep mode is disabled
            if (sleepMode == false && currentScreenJsonBuffer.length() > 0)
            {
                DynamicJsonBuffer jsonBuffer;
                JsonObject &tmpJson = jsonBuffer.parseObject(currentScreenJsonBuffer);
                CreateFrames(tmpJson);
                return;
            }
        }
    }

    // Ist eine Display Helligkeit übergeben worden?
    if (json.containsKey("brightness") && !sleepMode)
    {
        logMessage += F("Brightness Control, ");

        if (json["brightness"].as<int>() != currentMatrixBrightness)
        {
            sendMatrixInfo = true;
            currentMatrixBrightness = json["brightness"].as<int>();
            matrix->setBrightness(currentMatrixBrightness);
            matrix->show();
        }
    }

    // Set GPIO
    if (json.containsKey("setGpio"))
    {
        logMessage += F("Set Gpio, ");
        if (json["setGpio"]["set"].is<bool>() && json["setGpio"]["gpio"].is<uint8_t>())
        {
            uint8_t gpio = json["setGpio"]["gpio"].as<uint8_t>();

            // If the GPIO is already present in the array?
            // has been found, this is to be replaced.
            if (json["setGpio"]["duration"].is<ulong>())
            {
                int arrayIndex = -1;
                for (int i = 0; i < SET_GPIO_SIZE; i++)
                {
                    if (setGPIOReset[i].gpio == gpio)
                    {
                        arrayIndex = i;
                        break;
                    }
                }
                // Search free place in array.
                if (arrayIndex == -1)
                {
                    for (int i = 0; i < SET_GPIO_SIZE; i++)
                    {
                        if (setGPIOReset[i].gpio == -1)
                        {
                            arrayIndex = i;
                            break;
                        }
                    }
                }

                if (arrayIndex == -1)
                {
                    Log(F("SetGPIO"), F("Error: no free place in array found!"));
                }
                else
                {
                    // Save data in array for the reset.
                    setGPIOReset[arrayIndex].gpio = gpio;
                    setGPIOReset[arrayIndex].resetMillis = (millis() + json["setGpio"]["duration"].as<ulong>());
                    Log(F("SetGPIO"), "Pos: " + String(arrayIndex) + ", GPIO: " + String(gpio) + ", Duration: " + String(json["setGpio"]["duration"].as<char *>()) + ", Value: " + json["setGpio"]["set"].as<char *>());
                }
            }
            else
            {
                Log(F("SetGPIO"), "GPIO: " + String(gpio) + ", Value: " + json["setGpio"]["set"].as<char *>());
            }
            // Set GPIO
            pinMode(gpio, OUTPUT);
            digitalWrite(gpio, json["setGpio"]["set"].as<bool>());
        }
    }

    // Sound
    if (json.containsKey("sound"))
    {
        logMessage += F("Sound, ");
        // Volume
        if (json["sound"]["volume"] != NULL && json["sound"]["volume"].is<int>())
        {
            mp3Player.volume(json["sound"]["volume"].as<int>());

            // Sometimes, mp3Player gets hickups. A brief delay might help - but also might hinder scrolling.
            // So, do it only if there are more commands to come.
            if (json["sound"]["control"].as<String>() == "")
            {
                Log(F("Sound"), F("Changing volume can prevent DFPlayer from executing a control command at the same time. Better make two separate API calls."));
                delay(200);
            }
        }
        // Play
        if (json["sound"]["control"] == "play")
        {
            if (json["sound"]["folder"])
            {
                mp3Player.playFolder(json["sound"]["folder"].as<int>(), json["sound"]["file"].as<int>());
            }
            else
            {
                mp3Player.play(json["sound"]["file"].as<int>());
            }
        }
        // Stop
        else if (json["sound"]["control"] == "pause")
        {
            mp3Player.pause();
        }
        // Play Next
        else if (json["sound"]["control"] == "next")
        {
            mp3Player.playNext();
        }
        // Play Previous
        else if (json["sound"]["control"] == "previous")
        {
            mp3Player.playPrevious();
        }
    }

    // Process screen content if
    // - no sleepmode control is in message
    // - not sleeping
    // - no forced screen is active OR forceDuration is set
    if (!json.containsKey("sleepMode") && !sleepMode && (millis() >= forcedScreenIsActiveUntil || forceDuration > 0))
    {
        matrix->setBrightness(currentMatrixBrightness);

        // Prüfung für die Unterbrechnung der lokalen Schleifen
        if (json.containsKey("bitmap") || json.containsKey("bitmaps") || json.containsKey("text") || json.containsKey("bar") || json.containsKey("bars") || json.containsKey("bitmapAnimation"))
        {
            lastScreenMessageMillis = millis();
            clockAktiv = false;
            scrollTextAktivLoop = false;
            animateBMPAktivLoop = false;
        }

        // Ist eine Switch Animation übergeben worden?
        bool fadeAnimationAktiv = false;
        bool coloredBarWipeAnimationAktiv = false;
        bool zigzagWipeAnimationAktiv = false;
        bool bitmapWipeAnimationAktiv = false;
        if (json.containsKey("switchAnimation"))
        {
            logMessage += F("SwitchAnimation, ");
            // Switch Animation aktiv?
            if (json["switchAnimation"]["aktiv"])
            {
                // Fade Animation aktiv?
                if (json["switchAnimation"]["animation"] == "fade")
                {
                    fadeAnimationAktiv = true;
                }
                else if (json["switchAnimation"]["animation"] == "coloredBarWipe")
                {
                    coloredBarWipeAnimationAktiv = true;
                }
                else if (json["switchAnimation"]["animation"] == "zigzagWipe")
                {
                    zigzagWipeAnimationAktiv = true;
                }
                else if (json["switchAnimation"]["animation"] == "bitmapWipe")
                {
                    bitmapWipeAnimationAktiv = true;
                }
                else if (json["switchAnimation"]["animation"] == "random")
                {
                    switch (millis() % 3)
                    {
                    case 0:
                        fadeAnimationAktiv = true;
                        break;
                    case 1:
                        coloredBarWipeAnimationAktiv = true;
                        break;
                    case 2:
                        zigzagWipeAnimationAktiv = true;
                        break;
                    }
                }
            }
        }

        // Fade aktiv?
        if (fadeAnimationAktiv)
        {
            FadeOut();
        }
        else if (coloredBarWipeAnimationAktiv)
        {
            ColoredBarWipe();
        }
        else if (zigzagWipeAnimationAktiv)
        {
            uint8_t r = 255;
            uint8_t g = 255;
            uint8_t b = 255;
            if (json["switchAnimation"]["hexColor"].as<char *>() != NULL)
            {
                ColorConverter::HexToRgb(json["switchAnimation"]["hexColor"].as<char *>(), r, g, b);
            }
            else if (json["switchAnimation"]["color"]["r"].as<char *>() != NULL)
            {
                r = json["switchAnimation"]["color"]["r"].as<uint8_t>();
                g = json["switchAnimation"]["color"]["g"].as<uint8_t>();
                b = json["switchAnimation"]["color"]["b"].as<uint8_t>();
            }
            ZigZagWipe(r, g, b);
        }
        else if (bitmapWipeAnimationAktiv)
        {
            BitmapWipe(json["switchAnimation"]["data"].as<JsonArray>(), json["switchAnimation"]["width"].as<uint8_t>());
        }

        // Clock
        if (json.containsKey("clock"))
        {
            logMessage += F("InternalClock Control, Params: (");
            scrollTextAktivLoop = false;
            animateBMPAktivLoop = false;
            clockAktiv = true;

            clockCounterClock = 0;
            clockCounterDate = 0;

            bool isSwitchAktivSet = json["clock"]["switchAktiv"].is<bool>();
            if (isSwitchAktivSet)
            {
                logMessage += F("clockSwitchAktiv, ");
                clockSwitchAktiv = json["clock"]["switchAktiv"];
            }

            bool isClockSwitchSecSet = json["clock"]["switchSec"] != NULL;
            if (clockSwitchAktiv && isClockSwitchSecSet)
            {
                logMessage += F("clockSwitchSec, ");
                clockSwitchSec = json["clock"]["switchSec"];
            }

            bool isClockWithSecondsSet = json["clock"]["withSeconds"].is<bool>();
            if (isClockWithSecondsSet)
            {
                logMessage += F("withSeconds, ");
                clockWithSeconds = json["clock"]["withSeconds"];
            }

            bool isClockBlinkAnimatedSet = json["clock"]["blinkAnimated"].is<bool>();
            if (isClockBlinkAnimatedSet)
            {
                logMessage += F("blinkAnimated, ");
                clockBlinkAnimated = json["clock"]["blinkAnimated"];
            }

            bool isFatFontSet = json["clock"]["fatFont"].is<bool>();
            if (isFatFontSet)
            {
                logMessage += F("fatFont, ");
                clockFatFont = json["clock"]["fatFont"];
            }
            bool isLargeFontSet = json["clock"]["largeFont"].is<bool>();
            if (isLargeFontSet)
            {
                logMessage += F("largeFont, ");
                clockLargeFont = json["clock"]["largeFont"];
            }
            bool isDrawWeekDaysSet = json["clock"]["drawWeekDays"].is<bool>();
            if (isDrawWeekDaysSet)
            {
                logMessage += F("drawWeekDays, ");
                clockDrawWeekDays = json["clock"]["drawWeekDays"];
            }

            if (json["clock"]["color"]["r"].as<char *>() != NULL)
            {
                logMessage += F("color, ");
                clockColorR = json["clock"]["color"]["r"].as<uint8_t>();
                clockColorG = json["clock"]["color"]["g"].as<uint8_t>();
                clockColorB = json["clock"]["color"]["b"].as<uint8_t>();
            }
            else if (json["clock"]["hexColor"].as<char *>() != NULL)
            {
                logMessage += F("hexColor, ");
                ColorConverter::HexToRgb(json["clock"]["hexColor"].as<char *>(), clockColorR, clockColorG, clockColorB);
            }
            if (logMessage.endsWith(", "))
            {
                logMessage.remove(logMessage.length() - 2);
            }
            logMessage += F("), ");
            DrawClock(true);
        }

        if (json.containsKey("bitmap") || json.containsKey("bitmaps") || json.containsKey("bitmapAnimation") || json.containsKey("text") || json.containsKey("bar") || json.containsKey("bars"))
        {
            // Alle Pixel löschen
            matrix->clear();
        }

        // Bar
        if (json.containsKey("bar"))
        {
            logMessage += F("Bar, ");
            uint8_t r, g, b;
            if (json["bar"]["hexColor"].as<char *>() != NULL)
            {
                ColorConverter::HexToRgb(json["bar"]["hexColor"].as<char *>(), r, g, b);
            }
            else
            {
                r = json["bar"]["color"]["r"].as<uint8_t>();
                g = json["bar"]["color"]["g"].as<uint8_t>();
                b = json["bar"]["color"]["b"].as<uint8_t>();
            }
            matrix->drawLine(json["bar"]["position"]["x"], json["bar"]["position"]["y"], json["bar"]["position"]["x2"], json["bar"]["position"]["y2"], matrix->Color(r, g, b));
        }

        // Bars
        if (json.containsKey("bars"))
        {
            logMessage += F("Bars, ");
            for (JsonVariant x : json["bars"].as<JsonArray>())
            {
                uint8_t r, g, b;
                if (x["hexColor"].as<char *>() != NULL)
                {
                    ColorConverter::HexToRgb(x["hexColor"].as<char *>(), r, g, b);
                }
                else
                {
                    r = x["color"]["r"].as<uint8_t>();
                    g = x["color"]["g"].as<uint8_t>();
                    b = x["color"]["b"].as<uint8_t>();
                }
                matrix->drawLine(x["position"]["x"], x["position"]["y"], x["position"]["x2"], x["position"]["y2"], matrix->Color(r, g, b));
            }
        }

        // clear withBMP only if no sleepMode is in the same message
        if (!json.containsKey("sleepMode"))
        {
            withBMP = false;
        }

        // Restore withBMP from (stored) message
        if (json.containsKey("withBMPRestore"))
        {
            withBMP = json["withBMPRestore"];
            if (withBMP == true)
            {
                matrix->drawRGBBitmap(bmpPosX, bmpPosY, bmpArray, bmpWidth, bmpHeight);
            }
        }

        // Restore a animateBMPAktivLoop from (stored) message
        if (json.containsKey("animateBMPAktivLoopRestore"))
        {
            animateBMPAktivLoop = json["animateBMPAktivLoopRestore"];
        }

        // Ist ein Bitmap übergeben worden?
        if (json.containsKey("bitmap"))
        {
            logMessage += F("Bitmap, ");
            DrawSingleBitmap(json["bitmap"]);
        }

        // Sind mehrere Bitmaps übergeben worden?
        if (json.containsKey("bitmaps"))
        {
            logMessage += F("Bitmaps (");
            for (JsonVariant singleBitmap : json["bitmaps"].as<JsonArray>())
            {
                DrawSingleBitmap(singleBitmap);
                        }
            logMessage += json["bitmaps"].as<JsonArray>().size();
            logMessage += F("), ");
        }

        // Ist eine BitmapAnimation übergeben worden?
        if (json.containsKey("bitmapAnimation"))
        {
            bmpPosX = 0;
            bmpPosY = 0;
            bmpWidth = 8;
            bmpHeight = 8;
            if (json["bitmapAnimation"]["position"]["x"].is<int16_t>() && json["bitmapAnimation"]["position"]["y"].is<int16_t>())
            {
                bmpPosX = json["bitmapAnimation"]["position"]["x"].as<int16_t>();
                bmpPosY = json["bitmapAnimation"]["position"]["y"].as<int16_t>();
            }
            if (json["bitmapAnimation"]["size"]["width"].is<int16_t>() && json["bitmapAnimation"]["size"]["height"].is<int16_t>())
            {
                bmpWidth = json["bitmapAnimation"]["size"]["width"].as<int16_t>();
                bmpHeight = json["bitmapAnimation"]["size"]["height"].as<int16_t>();
            }
            withBMP = true;

            logMessage += F("BitmapAnimation, ");
            // animationBmpList zurücksetzten um das ende nacher zu finden -1 (habe aktuell keine bessere Lösung)
            for (int i = 0; i < 10; i++)
            {
                animationBmpList[i][0] = 2;
            }

            int counter = 0;
            for (JsonVariant x : json["bitmapAnimation"]["data"].as<JsonArray>())
            {
                // JsonArray in IntArray konvertieren
                x.as<JsonArray>().copyTo(bmpArray);
                // Speichern für die Ausgabe
                for (int i = 0; i < 64; i++)
                {
                    animationBmpList[counter][i] = bmpArray[i];
                }
                counter++;
            }

            // Serial.printf("Counter: %d\n", counter);

            animateBMPDelay = json["bitmapAnimation"]["animationDelay"];
            animateBMPRubberbandingAktiv = json["bitmapAnimation"]["rubberbanding"];

            animateBMPLimitLoops = 0;
            if (json["bitmapAnimation"]["limitLoops"])
            {
                animateBMPLimitLoops = json["bitmapAnimation"]["limitLoops"].as<int>();
            }

            // Hier einmal den Counter zurücksetzten
            animateBMPCounter = 0;
            animateBMPLoopCount = 0;
            animateBMPAktivLoop = true;
            animateBMPReverse = false;
            animateBMPPrevMillis = millis();
            AnimateBMP(false);
        }

        // Ist ein Text übergeben worden?
        bool scrollTextAktiv = false;
        if (json.containsKey("text"))
        {
            logMessage += F("Text, ");
            // Always assume the default delay first.
            scrollTextDelay = scrollTextDefaultDelay;

            // Is ScrollText auto or true selected?
            scrollTextAktiv = ((json["text"]["scrollText"] == "auto" || ((json["text"]["scrollText"]).is<bool>() && json["text"]["scrollText"])));

            uint8_t r, g, b;
            if (json["text"]["hexColor"].as<char *>() != NULL)
            {
                ColorConverter::HexToRgb(json["text"]["hexColor"].as<char *>(), r, g, b);
            }
            else
            {
                r = json["text"]["color"]["r"].as<uint8_t>();
                g = json["text"]["color"]["g"].as<uint8_t>();
                b = json["text"]["color"]["b"].as<uint8_t>();
            }

            // Is ScrollText auto or true selected?
            if (scrollTextAktiv)
            {

                bool centerText = json["text"]["centerText"];

                bool fadeInRequired = ((json.containsKey("bars") || json.containsKey("bar") || json.containsKey("bitmap") || json.containsKey("bitmapAnimation")) && fadeAnimationAktiv);

                // Wurde ein Benutzerdefeniertes Delay übergeben?
                if (json["text"]["scrollTextDelay"])
                {
                    scrollTextDelay = json["text"]["scrollTextDelay"];
                }

                if (!(json["text"]["scrollText"]).is<bool>() && json["text"]["scrollText"] == "auto")
                {
                    DrawAutoTextScrolled(json["text"]["textString"], json["text"]["bigFont"], centerText, fadeInRequired, r, g, b, json["text"]["position"]["x"], json["text"]["position"]["y"]);
                }
                else
                {
                    DrawTextScrolled(json["text"]["textString"], json["text"]["bigFont"], centerText, fadeInRequired, r, g, b, json["text"]["position"]["x"], json["text"]["position"]["y"]);
                }
            }
            // is centerText selected?
            else if (json["text"]["centerText"])
            {
                DrawTextCenter(json["text"]["textString"], json["text"]["bigFont"], r, g, b, json["text"]["position"]["x"], json["text"]["position"]["y"]);
            }
            else
            {
                DrawText(json["text"]["textString"], json["text"]["bigFont"], r, g, b, json["text"]["position"]["x"], json["text"]["position"]["y"]);
            }
        }

        // Fade aktiv?
        if (!scrollTextAktiv && (fadeAnimationAktiv || coloredBarWipeAnimationAktiv || zigzagWipeAnimationAktiv || bitmapWipeAnimationAktiv))
        {
            FadeIn();
        }
        else
        {
            // Fade nicht aktiv!
            // Muss mich selbst um Show kümmern
            matrix->show();
        }
    }

    if (sleepMode && !json.containsKey("sleepMode"))
    {
        logMessage += F("[not all data processed because sleepMode is active]");
    }

    if (logMessage.endsWith(", "))
    {
        logMessage.remove(logMessage.length() - 2);
        logMessage += F(" ");
    }
    Log(F("CreateFrames"), logMessage + "(Length: " + json.measureLength() + ")");

    if (forceDuration > 0 && (json.containsKey("bitmap") || json.containsKey("bitmaps") || json.containsKey("text") || json.containsKey("bar") || json.containsKey("bars") || json.containsKey("bitmapAnimation")))
    {
        forcedScreenIsActiveUntil = millis() + forceDuration;
    }

    if (!json.containsKey("sleepMode") && !sleepMode)
    {
        // Store last frame

        json.remove("bitmap");
        json.remove("bitmaps");
        json.remove("bitmapAnimation");
        json["withBMPRestore"] = withBMP;
        json["animateBMPAktivLoopRestore"] = animateBMPAktivLoop;
        currentScreenJsonBuffer = "";
        json.printTo(currentScreenJsonBuffer);
    }

    if (sendMatrixInfo)
    {
        SendMatrixInfo();
    }
}

String GetConfig()
{
#if defined(ESP8266)
    File configFile = LittleFS.open("/config.json", "r");
#elif defined(ESP32)
    File configFile = SPIFFS.open("/config.json", "r");
#endif

    if (configFile)
    {
        // Log(F("GetConfig"), F("Opened config file"));
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonBuffer jsonBuffer;
        JsonObject &root = jsonBuffer.parseObject(buf.get());

        String json;
        root.printTo(json);

        return json;
    }
    return "";
}

String GetSensor()
{
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();
    if (tempSensor == TempSensor_BME280)
    {
        const float currentTemp = bme280->readTemperature();
        root["temperature"] = currentTemp + temperatureOffset;
        root["humidity"] = bme280->readHumidity() + humidityOffset;
        root["pressure"] = (bme280->readPressure() / 100.0F) + pressureOffset;
        root["gas"] = "Not installed";

        if (temperatureUnit == TemperatureUnit_Fahrenheit)
        {
            root["temperature"] = CelsiusToFahrenheit(currentTemp) + temperatureOffset;
        }
    }
    else if (tempSensor == TempSensor_DHT)
    {
        const float currentTemp = dht.getTemperature();
        root["temperature"] = currentTemp + temperatureOffset;
        root["humidity"] = roundf(dht.getHumidity() + humidityOffset);
        root["pressure"] = "Not installed";
        root["gas"] = "Not installed";

        if (temperatureUnit == TemperatureUnit_Fahrenheit)
        {
            root["temperature"] = CelsiusToFahrenheit(currentTemp) + temperatureOffset;
        }
    }
    else if (tempSensor == TempSensor_BME680)
    {
        /***************************************************************************************************
        // BME680 requires about 100ms for a read (heating the gas sensor). A blocking read can hinder
        // animations and scrolling. Therefore, we will use asynchronous reading in most cases.
        //
        // First call: starts measuring sequence, returns previous values.
        // Second call: performs read, returns current values.
        //
        // As long as there are more than ~200ms between the calls, there won't be blocking.
        // PixelIt usually uses a 3000ms loop.
        //
        // When there's no loop (no Websock connection, no MQTT) but only HTTP API calls, this would result
        // in only EVERY OTHER call return new values (which have been taken shortly after the previous call).
        // This is okay when you are polling very frequently, but might be undesirable when polling every
        // couple of minutes or so. Therefore: if previous reading is more than 20000ms old, perform
        // read in any case, even if it might become blocking.
        //
        // Please note: the gas value not only depends on gas, but also on the time since last read.
        // Frequent reads will yield higher values than infrequent reads. There will be a difference
        // even if we switch from 6secs to 3secs! So, do not attempt to compare values of readings
        // with an interval of 3 secs to values of readings with an interval of 60 secs!
        */

        const int elapsedSinceLastRead = millis() - lastBME680read;
        const int remain = bme680->remainingReadingMillis();

        if (remain == -1) // no current values available
        {
            bme680->beginReading(); // start measurement process
            // return previous values
            const float currentTemp = bme680->temperature;
            root["temperature"] = currentTemp + temperatureOffset;
            root["humidity"] = bme680->humidity + humidityOffset;
            root["pressure"] = (bme680->pressure / 100.0F) + pressureOffset;
            root["gas"] = (bme680->gas_resistance / 1000.0F) + gasOffset;
            if (temperatureUnit == TemperatureUnit_Fahrenheit)
            {
                root["temperature"] = CelsiusToFahrenheit(currentTemp) + temperatureOffset;
            }
        }

        if (remain >= 0 || elapsedSinceLastRead > 20000)
        // remain==0: measurement completed, not read yet
        // remain>0: measurement still running, but as we already are in the next loop call, block and read
        // elapsedSinceLastRead>20000: obviously, remain==-1. But as there haven't been loop calls recently, this seems to be an "infrequent" API call. Perform blocking read.
        {
            if (bme680->endReading()) // will become blocking if measurement not complete yet
            {
                lastBME680read = millis();
                const float currentTemp = bme680->temperature;
                root["temperature"] = currentTemp + temperatureOffset;
                root["humidity"] = bme680->humidity + humidityOffset;
                root["pressure"] = (bme680->pressure / 100.0F) + pressureOffset;
                root["gas"] = (bme680->gas_resistance / 1000.0F) + gasOffset;
                if (temperatureUnit == TemperatureUnit_Fahrenheit)
                {
                    root["temperature"] = CelsiusToFahrenheit(currentTemp) + temperatureOffset;
                }
            }
            else
            {
                root["humidity"] = "Error while reading";
                root["temperature"] = "Error while reading";
                root["pressure"] = "Error while reading";
                root["gas"] = "Error while reading";
            }
        }
    }
    else if (tempSensor == TempSensor_BMP280)
    {
        const float currentTemp = bmp280->readTemperature();
        root["temperature"] = currentTemp + temperatureOffset;
        // root["humidity"] = bmp280->readHumidity() + humidityOffset;
        root["humidity"] = "Not installed";
        root["pressure"] = (bmp280->readPressure() / 100.0F) + pressureOffset;
        root["gas"] = "Not installed";

        if (temperatureUnit == TemperatureUnit_Fahrenheit)
        {
            root["temperature"] = CelsiusToFahrenheit(currentTemp) + temperatureOffset;
        }
    }
    else if (tempSensor == TempSensor_SHT31)
    {
        const float currentTemp = sht31.readTemperature();
        const float currentHumi = sht31.readHumidity();
        root["temperature"] = currentTemp + temperatureOffset;
        root["humidity"] = roundf(currentHumi + humidityOffset);
        root["pressure"] = "Not installed";
        root["gas"] = "Not installed";
    }
    else
    {
        root["humidity"] = "Not installed";
        root["temperature"] = "Not installed";
        root["pressure"] = "Not installed";
        root["gas"] = "Not installed";
    }

    if (VBAT_PIN > 0)
    {
        root["battery"] = batteryLevel;
    }
    else
    {
        root["battery"] = "Not installed";
    }
    root["hostname"] = hostname;

    String json;
    root.printTo(json);

    // Log(F("Sensor readings"), F("Hum/Temp/Press/Gas:"));
    // Log(F("Sensor readings"), json);
    return json;
}

String GetLuxSensor()
{
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    root["lux"] = currentLux;
    root["hostname"] = hostname;

    String json;
    root.printTo(json);

    return json;
}

String GetBrightness()
{
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    root["brightness_255"] = currentMatrixBrightness;
    root["brightness"] = map(currentMatrixBrightness, 0, 255, 0, 100);
    root["hostname"] = hostname;

    String json;
    root.printTo(json);

    return json;
}

String GetMatrixInfo()
{
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    root["pixelitVersion"] = VERSION;
    //// Matrix Config
    root["note"] = note;
    root["hostname"] = hostname;
    root["deviceID"] = deviceID;
    root["buildSection"] = STR(BUILD_SECTION);
    root["freeSketchSpace"] = ESP.getFreeSketchSpace();
    root["wifiRSSI"] = WiFi.RSSI();
    root["wifiQuality"] = GetRSSIasQuality(WiFi.RSSI());
    root["wifiSSID"] = WiFi.SSID();
    root["ipAddress"] = WiFi.localIP().toString();
    root["freeHeap"] = ESP.getFreeHeap();
    root["currentMatrixBrightness"] = currentMatrixBrightness;
    root["wifiBSSID"] = WiFi.BSSIDstr();

#if defined(ESP8266)
    root["sketchSize"] = ESP.getSketchSize();
    root["chipID"] = ESP.getChipId();
#elif defined(ESP32)
    root["chipID"] = uint64ToString(ESP.getEfuseMac());
#endif

    root["cpuFreqMHz"] = ESP.getCpuFreqMHz();
    root["sleepMode"] = sleepMode;
    root["uptime"] = millis() / 1000;
    root["resetReason"] = ResetReason();
    JsonObject &matrix = root.createNestedObject("matrixsize");
    matrix["cols"] = MATRIX_WIDTH;
    matrix["rows"] = MATRIX_HEIGHT;

    String json;
    root.printTo(json);

    return json;
}

String GetButtons()
{
    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    for (uint button = 0; button < 3; button++)
    {
        root[btnAPINames[button]] = btnLastPublishState[button] ? "true" : "false";
    }
    root["hostname"] = hostname;

    String json;
    root.printTo(json);

    return json;
}

void SendTelemetry()
{
    Log(F("SendTelemetry"), F("Sending..."));
    HttpClient httpClient = HttpClient(wifiClientHTTP, TELEMETRY_SERVER_HOST, TELEMETRY_SERVER_PORT);
    httpClient.setTimeout(1500);
    httpClient.sendHeader("User-Agent", "PixelIt");
    httpClient.post(TELEMETRY_SERVER_PATH, "application/json", GetTelemetry());
}

String GetTelemetry()
{
    const String MatrixTypeNames[] = {"Colum major", "Row major", "Tiled 4x 8x8 CJMCU (Column major)", "MicroMatrix", "Tiled 4x 8x8 CJMCU (Row major)"};
    const String TempSensorNames[] = {"none", "BME280", "DHT", "BME680", "BMP280", "SHT31"};
    const String LuxSensorNames[] = {"LDR", "BH1750", "Max44009"};

    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    root["uuid"] = sha1(GetChipID());
    root["version"] = VERSION;
    root["type"] = isESP8266 ? "esp8266" : "esp32";
    root["buildSection"] = STR(BUILD_SECTION);

    JsonObject &matrix = root.createNestedObject("matrix");
    matrix["type"] = matrixType;
    matrix["name"] = MatrixTypeNames[matrixType - 1];
    matrix["width"] = MATRIX_WIDTH;
    matrix["height"] = MATRIX_HEIGHT;

    JsonArray &sensors = root.createNestedArray("sensors");
    sensors.add(LuxSensorNames[luxSensor]);
    if (tempSensor > 0)
    {
        sensors.add(TempSensorNames[tempSensor]);
    }

    String json;
    root.printTo(json);
    return json;
}

void DrawText(String text, int bigFont, int colorRed, int colorGreen, int colorBlue, int posX, int posY)
{
    DrawTextHelper(text, bigFont, false, false, false, false, colorRed, colorGreen, colorBlue, posX, posY);
}

void DrawTextCenter(String text, int bigFont, int colorRed, int colorGreen, int colorBlue, int posX, int posY)
{
    DrawTextHelper(text, bigFont, true, false, false, false, colorRed, colorGreen, colorBlue, posX, posY);
}

void DrawTextScrolled(String text, int bigFont, bool centerText, bool fadeInRequired, int colorRed, int colorGreen, int colorBlue, int posX, int posY)
{
    DrawTextHelper(text, bigFont, centerText, true, false, fadeInRequired, colorRed, colorGreen, colorBlue, posX, posY);
}

void DrawAutoTextScrolled(String text, int bigFont, bool centerText, bool fadeInRequired, int colorRed, int colorGreen, int colorBlue, int posX, int posY)
{
    DrawTextHelper(text, bigFont, centerText, false, true, fadeInRequired, colorRed, colorGreen, colorBlue, posX, posY);
}

void DrawTextHelper(String text, int bigFont, bool centerText, bool scrollText, bool autoScrollText, bool fadeInRequired, int colorRed, int colorGreen, int colorBlue, int posX, int posY)
{
    uint16_t xTextWidth, xAvailableTextSpace;
    int16_t boundsx1, boundsy1;
    uint16_t boundsw, boundsh;

    text = Utf8ToAscii(text);

    // Debug function values
    // Serial.printf("DrawTextHelper: %s (bigFont: %s, centerText: %s, scrollText: %s, autoScrollText: %s: %s, fadeInRequired: %s)\n", text.c_str(), bigFont == 1 ? "true" : "false", centerText ? "true" : "false", scrollText ? "true" : "false", autoScrollText ? "true" : "false" ? "true" : "false", fadeInRequired ? "true" : "false");

    // Available text space depends on matrix size
    xAvailableTextSpace = MATRIX_WIDTH - posX;

    if (bigFont == 1)
    {
        // Set large font
        matrix->setFont();
        matrix->getTextBounds(text, 0, 0, &boundsx1, &boundsy1, &boundsw, &boundsh);
        xTextWidth = boundsw;

        // Position correction
        posY = posY - 1;
    }
    else if (bigFont == 2) // fat font, only to be used for time display
    {
        // Set fat font
        matrix->setFont(&FatPixels);

        matrix->getTextBounds(text, 0, 0, &boundsx1, &boundsy1, &boundsw, &boundsh);
        xTextWidth = boundsw;

        // Position correction
        posY = posY + 6;
    }
    else if (bigFont == 3) // very large font, only to be used for time display
    {
        // Set very large font
        matrix->setFont(&LargePixels);

        matrix->getTextBounds(text, 0, 0, &boundsx1, &boundsy1, &boundsw, &boundsh);
        xTextWidth = boundsw;

        // Position correction
        posY = posY + 6;
    }
    else
    {
        // Set small font
        matrix->setFont(&PixelItFont);
        matrix->getTextBounds(text, 0, 0, &boundsx1, &boundsy1, &boundsw, &boundsh);
        xTextWidth = boundsw - 4;

        // Position correction
        posY = posY + 5;
    }

    if (centerText && xTextWidth < xAvailableTextSpace) // center text if text is smaller than available pixels
    {
        // Calculation for the first pixel of the text
        posX = posX + ((xAvailableTextSpace - xTextWidth) / 2);
    }

    matrix->setCursor(posX, posY);

    matrix->setTextColor(matrix->Color(colorRed, colorGreen, colorBlue));

    if (scrollText || (autoScrollText && xTextWidth > xAvailableTextSpace)) // scroll text if text is larger than available pixels
    {

        matrix->setBrightness(currentMatrixBrightness);

        scrollTextString = text;
        scrollposY = posY;
        scrollposX = posX;
        scrollxTextWidth = xTextWidth;
        scrollxAvailableTextSpace = xAvailableTextSpace;
        scrollCurPos = MATRIX_WIDTH + 1;

        scrollTextAktivLoop = true;
        scrollTextPrevMillis = millis();
        ScrollText(fadeInRequired);
    }
    // In case the text on the display fits!
    else if (autoScrollText)
    {
        matrix->print(text);
        // Here you have to check if the fade out was done,
        // then the brightness is too low (0),
        // but only if animateBMPAktivLoop is not active,
        // because otherwise it's already done.
        if (fadeInRequired && !animateBMPAktivLoop)
        {
            FadeIn();
        }
        else
        {
            matrix->show();
        }
    }
    else
    {
        matrix->print(text);
    }
}

void ScrollText(bool isFadeInRequired)
{
    int xOffset = MATRIX_WIDTH - scrollxAvailableTextSpace;

    if (scrollCurPos > ((scrollxTextWidth - xOffset) * -1))
    {
        matrix->clear();
        matrix->setCursor(--scrollCurPos, scrollposY);
        matrix->print(scrollTextString);

        // draw black pixel under icon / blank space if (xOffset > 0)
        for (int i = 0; i < xOffset; i++)
        {
            matrix->drawLine(i, 0, i, MATRIX_HEIGHT, matrix->Color(0, 0, 0));
        }

        if (withBMP)
        {
            matrix->drawRGBBitmap(bmpPosX, bmpPosY, bmpArray, bmpWidth, bmpHeight);
        }

        if (isFadeInRequired)
        {
            FadeIn();
        }
        else
        {
            matrix->show();
        }
    }
    else
    {
        scrollCurPos = MATRIX_WIDTH;
    }
}

void AnimateBMP(bool isShowRequired)
{
    // Prüfen auf 2, 2 ist mein Platzhalter für leeres obj!
    if (animationBmpList[animateBMPCounter][0] == 2)
    {
        // Ist ein Repeat Limit übergeben worden.
        if (animateBMPLimitLoops > 0 && !animateBMPRubberbandingAktiv)
        {
            animateBMPLoopCount++;

            // Ist der Repeat Limit erreicht den AnimateBMP Loop deaktiveren.
            if (animateBMPLoopCount == animateBMPLimitLoops)
            {
                animateBMPAktivLoop = false;
                return;
            }
        }

        // Prüfen ob Rubberbanding aktiv und mehr wie 1 Frame vorhanden ist.
        if (animateBMPRubberbandingAktiv && animateBMPCounter > 1)
        {
            animateBMPReverse = true;

            // 2 abziehen da sonst der letzte Frame doppelt angezeigt wird.
            if (animateBMPCounter > 0)
            {
                animateBMPCounter = animateBMPCounter - 2;
            }
        }
        else
        {
            animateBMPCounter = 0;
        }
    }

    if (animateBMPCounter < 0)
    {
        // Auf 1 setzten da sons der erste Frame doppelt angezeigt wird.
        animateBMPCounter = 1;
        animateBMPReverse = false;

        // Ist ein Repeat Limit übergeben worden.
        if (animateBMPLimitLoops > 0)
        {
            animateBMPLoopCount++;
            // Ist der Repeat Limit erreicht den AnimateBMP Loop deaktiveren.
            if (animateBMPLoopCount >= animateBMPLimitLoops)
            {
                animateBMPAktivLoop = false;
                return;
            }
        }
    }

    ClearBMPArea();

    matrix->drawRGBBitmap(bmpPosX, bmpPosY, animationBmpList[animateBMPCounter], bmpWidth, bmpHeight);

    for (int y = 0; y < 64; y++)
    {
        bmpArray[y] = animationBmpList[animateBMPCounter][y];
    }

    // Soll der Loop wieder zurücklaufen?
    if (animateBMPReverse)
    {
        animateBMPCounter--;
    }
    else
    {
        animateBMPCounter++;
    }

    if (isShowRequired)
    {
        matrix->show();
    }
}

void DrawSingleBitmap(JsonObject &json)
{
    int16_t h = json["size"]["height"].as<int16_t>();
    int16_t w = json["size"]["width"].as<int16_t>();
    int16_t x = json["position"]["x"].as<int16_t>();
    int16_t y = json["position"]["y"].as<int16_t>();

    bmpHeight = h;
    bmpWidth = w;
    bmpPosX = x;
    bmpPosY = y;
    withBMP = true;

    // Hier kann leider nicht die Funktion matrix->drawRGBBitmap() genutzt werde da diese Fehler in der Anzeige macht wenn es mehr wie 8x8 Pixel werden.
    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            matrix->drawPixel(x + i, y, json["data"][j * w + i].as<uint16_t>());
        }
    }

    // JsonArray in IntArray konvertieren
    // dies ist nötig für diverse kleine Logiken z.B. Scrolltext
    // bei Multibitmaps landet hier nur eine der Bitmaps - das ist aber egal, da dann eh nicht gescrollt wird
    json["data"].as<JsonArray>().copyTo(bmpArray);
}

void DrawClock(bool fromJSON)
{
    matrix->clear();

    char date[14];
    char time[14];

    int xPosTime = 0;

    // use Adafruit 7px font as large clock font by default
    uint clockFontChoice = 1;
    if (clockFatFont) // use fat 8px clock font if set explicitly
    {
        clockFontChoice = 2;
    }
    else if (clockLargeFont) // use large 8px clock font if set explicitly
    {
        clockFontChoice = 3;
    }

    // check if a large font is set
    bool clockFontIsLarge = false;
    if (clockLargeFont || clockFatFont)
    {
        clockFontIsLarge = true;
    }

    if (clockDateDayMonth)
    {
        sprintf_P(date, PSTR("%02d.%02d."), day(), month());
    }
    else
    {
        sprintf_P(date, PSTR("%02d/%02d"), month(), day());
    }

    if (clock24Hours && clockWithSeconds && !clockFontIsLarge)
    {
        xPosTime = 2;
        sprintf_P(time, PSTR("%02d:%02d:%02d"), hour(), minute(), second());
    }
    else if (!clock24Hours)
    {
        xPosTime = 2;

        if (clockBlink && clockBlinkAnimated)
        {
            clockBlink = false;
            if (!clockFontIsLarge)
            {
                sprintf_P(time, PSTR("%2d %02d %s"), hourFormat12(), minute(), isAM() ? "AM" : "PM");
            }
            else
            {
                sprintf_P(time, PSTR("%2d %02d"), hourFormat12(), minute());
            }
        }
        else
        {
            clockBlink = !clockBlink;
            if (!clockFontIsLarge)
            {
                sprintf_P(time, PSTR("%2d:%02d %s"), hourFormat12(), minute(), isAM() ? "AM" : "PM");
            }
            else
            {
                sprintf_P(time, PSTR("%2d:%02d"), hourFormat12(), minute());
            }
        }
    }
    else
    {
        xPosTime = 7;

        if (clockBlink && clockBlinkAnimated)
        {
            clockBlink = false;
            sprintf_P(time, PSTR("%02d %02d"), hour(), minute());
        }
        else
        {
            clockBlink = !clockBlink;
            sprintf_P(time, PSTR("%02d:%02d"), hour(), minute());
        }
    }

    if (!clockSwitchAktiv || (clockSwitchAktiv && clockCounterClock <= clockSwitchSec))
    {
        if (clockSwitchAktiv)
        {
            clockCounterClock++;
        }
        if (clockCounterClock > clockSwitchSec)
        {
            clockCounterDate = 0;

            if (clockFontIsLarge) // fade rather than vertical animate purely because DrawTextCenter doesnt have a Y argument...
            {
                DrawTextCenter(String(time), clockFontChoice, clockColorR, clockColorG, clockColorB, 0, 1);
                FadeOut(30);
                matrix->clear();
                DrawTextCenter(String(date), clockFontChoice, clockColorR, clockColorG, clockColorB, 0, 1);
                FadeIn(30);
            }
            else
            {
                int counter = 0;
                while (counter <= 6) // vertical animate
                {
                    counter++;
                    matrix->clear();
                    DrawText(String(time), false, clockColorR, clockColorG, clockColorB, xPosTime, (1 + counter));
                    DrawText(String(date), false, clockColorR, clockColorG, clockColorB, 7, (-6 + counter));
                    matrix->drawLine(0, 7, 33, 7, 0);
                    if (clockDrawWeekDays)
                    {
                        DrawWeekDay();
                    }
                    matrix->show();
                    delay(35);
                }
            }
        }
        else if (clockFontIsLarge)
        {

            DrawTextCenter(String(time), clockFontChoice, clockColorR, clockColorG, clockColorB, 0, 1);
        }
        else
        {
            DrawText(String(time), false, clockColorR, clockColorG, clockColorB, xPosTime, 1);
            xPosTime = 3;
        }
    }
    else
    {
        clockCounterDate++;

        if (clockCounterDate == clockSwitchSec)
        {
            clockCounterClock = 0;

            if (clockFontIsLarge) // fade rather than vertical animate purely because DrawTextCenter doesnt have a Y argument...
            {
                DrawTextCenter(String(date), 2, clockColorR, clockColorG, clockColorB, 0, 1);
                FadeOut(30);
                matrix->clear();
                DrawTextCenter(String(time), 2, clockColorR, clockColorG, clockColorB, 0, 1);
                FadeIn(30);
            }
            else
            {
                int counter = 0;
                while (counter <= 6) // vertical animate
                {
                    counter++;
                    matrix->clear();
                    DrawText(String(date), false, clockColorR, clockColorG, clockColorB, 7, (1 + counter));
                    DrawText(String(time), false, clockColorR, clockColorG, clockColorB, xPosTime, (-6 + counter));
                    matrix->drawLine(0, 7, 33, 7, 0);
                    if (clockDrawWeekDays)
                    {
                        DrawWeekDay();
                    }
                    matrix->show();
                    delay(35);
                }
            }
        }
        else if (clockFontIsLarge)
        {
            DrawTextCenter(String(date), clockFontChoice, clockColorR, clockColorG, clockColorB, 0, 1);
        }
        else
        {
            DrawText(String(date), false, clockColorR, clockColorG, clockColorB, 7, 1);
        }
    }

    if (!clockFontIsLarge && clockDrawWeekDays)
    {
        DrawWeekDay();
    }

    // Wenn der Aufruf nicht über JSON sondern über den Loop kommt
    // muss ich mich selbst ums Show kümmern.
    if (!fromJSON)
    {
        matrix->show();
    }
}

void DrawWeekDay()
{
    // The Libary works with dayOfWeek with Sunday = 1...
    // So Sunday = 1 <-> Saturday = 7
    int weekDayNumber = 0;
    if (clockDayOfWeekFirstMonday)
    {
        weekDayNumber = DayOfWeekFirstMonday(dayOfWeek(now()) - 1);
    }
    else
    {
        weekDayNumber = dayOfWeek(now()) - 1;
    }

    for (int i = 0; i <= 6; i++)
    {
        if (i == weekDayNumber)
        {
            matrix->drawLine(2 + i * 4, 7, i * 4 + 4, 7, matrix->Color(clockColorR, clockColorG, clockColorB));
        }
        else
        {
            matrix->drawLine(2 + i * 4, 7, i * 4 + 4, 7, 21162);
        }
    }
}

boolean MQTTreconnect()
{

    bool connected = false;
    if (mqttUser != NULL && mqttUser.length() > 0 && mqttPassword != NULL && mqttPassword.length() > 0)
    {
        Log(F("MQTTreconnect"), F("MQTT connecting to broker..."));
        connected = client.connect(hostname.c_str(), mqttUser.c_str(), mqttPassword.c_str(), (mqttMasterTopic + "state").c_str(), 0, true, "disconnected");
    }
    else
    {
        Log(F("MQTTreconnect"), F("MQTT connecting to broker..."));
        connected = client.connect(hostname.c_str(), (mqttMasterTopic + "state").c_str(), 0, true, "disconnected");
    }

    if (connected)
    {
        Log(F("MQTTreconnect"), F("MQTT connected!"));
        // Subscribe to topics ...
        client.subscribe((mqttMasterTopic + "setScreen").c_str());
        client.subscribe((mqttMasterTopic + "getLuxsensor").c_str());
        client.subscribe((mqttMasterTopic + "getMatrixinfo").c_str());
        client.subscribe((mqttMasterTopic + "getConfig").c_str());
        client.subscribe((mqttMasterTopic + "setConfig").c_str());
        if (mqttUseDeviceTopic)
        {
            client.subscribe((mqttDeviceTopic + "setScreen").c_str());
            client.subscribe((mqttDeviceTopic + "getLuxsensor").c_str());
            client.subscribe((mqttDeviceTopic + "getMatrixinfo").c_str());
            client.subscribe((mqttDeviceTopic + "getConfig").c_str());
            client.subscribe((mqttDeviceTopic + "setConfig").c_str());
        }
        // ... and publish state ....
        client.publish((mqttMasterTopic + "state").c_str(), "connected", true);
        if (mqttUseDeviceTopic)
        {
            client.publish((mqttDeviceTopic + "state").c_str(), "connected", true);
        }

        // ... and provide discovery information
        // Create discovery information for Homeassistant
        // Can also be processed by ioBroker, OpenHAB etc.
        if (mqttHAdiscoverable)
        {

            // Get host IP to provide URL in MQTT discovery device info
            String ip_url = "http://" + WiFi.localIP().toString();
            String configTopicTemplate = String(F("homeassistant/#COMPONENT#/#DEVICEID#/#DEVICEID##SENSORID#/config"));
            configTopicTemplate.replace(F("#DEVICEID#"), deviceID);
            String configPayloadTemplate = String(F(
                "{"
                "\"dev\":{"
                "\"ids\":\"#DEVICEID#\","
                "\"name\":\"#HOSTNAME#\","
                "\"mdl\":\"PixelIt\","
                "\"mf\":\"PixelIt\","
                "\"sw\":\"#VERSION#\","
                "\"cu\":\"#IP#\""
                "},"
                "\"avty_t\":\"#MASTERTOPIC#state\","
                "\"pl_avail\":\"connected\","
                "\"pl_not_avail\":\"disconnected\","
                "\"uniq_id\":\"#DEVICEID##SENSORID#\","
                "\"dev_cla\":\"#CLASS#\","
                "\"name\":\"#SENSORNAME#\","
                "\"stat_t\":\"#MASTERTOPIC##STATETOPIC#\","
                "\"unit_of_meas\":\"#UNIT#\","
                "\"val_tpl\":\"{{value_json.#VALUENAME#}}\""
                "}"));
            configPayloadTemplate.replace(" ", "");
            configPayloadTemplate.replace(F("#DEVICEID#"), deviceID);
            configPayloadTemplate.replace(F("#HOSTNAME#"), hostname);
            configPayloadTemplate.replace(F("#VERSION#"), VERSION);
            if (mqttUseDeviceTopic)
            {
                configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttDeviceTopic);
            }
            else
            {
                configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttMasterTopic);
            }
            configPayloadTemplate.replace(F("#IP#"), ip_url);

            String topic;
            String payload;

            if (tempSensor != TempSensor_None)
            {
                topic = configTopicTemplate;
                topic.replace(F("#COMPONENT#"), F("sensor"));
                topic.replace(F("#SENSORID#"), F("Temperature"));

                payload = configPayloadTemplate;
                payload.replace(F("#SENSORID#"), F("Temperature"));
                payload.replace(F("#SENSORNAME#"), F("Temperature"));
                payload.replace(F("#CLASS#"), F("temperature"));
                payload.replace(F("#STATETOPIC#"), F("sensor"));
                payload.replace(F("#UNIT#"), "°C");
                payload.replace(F("#VALUENAME#"), F("temperature"));
                client.publish(topic.c_str(), payload.c_str(), true);

                topic = configTopicTemplate;
                topic.replace(F("#COMPONENT#"), F("sensor"));
                topic.replace(F("#SENSORID#"), F("Humidity"));

                payload = configPayloadTemplate;
                payload.replace(F("#SENSORNAME#"), F("Humidity"));
                payload.replace(F("#CLASS#"), F("humidity"));
                payload.replace(F("#STATETOPIC#"), F("sensor"));
                payload.replace(F("#UNIT#"), "%");
                payload.replace(F("#VALUENAME#"), F("humidity"));
                client.publish(topic.c_str(), payload.c_str(), true);
            }
            if (tempSensor == TempSensor_BME280 || tempSensor == TempSensor_BMP280 || tempSensor == TempSensor_BME680)
            {
                topic = configTopicTemplate;
                topic.replace(F("#COMPONENT#"), F("sensor"));
                topic.replace(F("#SENSORID#"), F("Pressure"));

                payload = configPayloadTemplate;
                payload.replace(F("#SENSORID#"), F("Pressure"));
                payload.replace(F("#SENSORNAME#"), F("Pressure"));
                payload.replace(F("#CLASS#"), F("pressure"));
                payload.replace(F("#STATETOPIC#"), F("sensor"));
                payload.replace(F("#UNIT#"), "hPa");
                payload.replace(F("#VALUENAME#"), F("pressure"));
                client.publish(topic.c_str(), payload.c_str(), true);
            }

            if (tempSensor == TempSensor_BME680)
            {
                topic = configTopicTemplate;
                topic.replace(F("#COMPONENT#"), F("sensor"));
                topic.replace(F("#SENSORID#"), F("VOC"));

                payload = configPayloadTemplate;
                payload.replace(F("#SENSORID#"), F("VOC"));
                payload.replace(F("#SENSORNAME#"), F("VOC"));
                payload.replace(F("#CLASS#"), F("volatile_organic_compounds"));
                payload.replace(F("#STATETOPIC#"), F("sensor"));
                payload.replace(F("#UNIT#"), "kOhm");
                payload.replace(F("#VALUENAME#"), F("gas"));
                client.publish(topic.c_str(), payload.c_str(), true);
            }
            topic = configTopicTemplate;
            topic.replace(F("#COMPONENT#"), F("sensor"));
            topic.replace(F("#SENSORID#"), F("Illuminance"));

            payload = configPayloadTemplate;
            payload.replace(F("#SENSORID#"), F("Illuminance"));
            payload.replace(F("#SENSORNAME#"), F("Illuminance"));
            payload.replace(F("#CLASS#"), F("illuminance"));
            payload.replace(F("#STATETOPIC#"), F("luxsensor"));
            payload.replace(F("#UNIT#"), "lx");
            payload.replace(F("#VALUENAME#"), F("lux"));
            client.publish(topic.c_str(), payload.c_str(), true);

            configPayloadTemplate = String(F(
                "{"
                "\"dev\":{"
                "\"ids\":\"#DEVICEID#\","
                "\"name\":\"#HOSTNAME#\","
                "\"mdl\":\"PixelIt\","
                "\"mf\":\"PixelIt\","
                "\"sw\":\"#VERSION#\","
                "\"cu\":\"#IP#\""
                "},"
                "\"avty_t\":\"#MASTERTOPIC#state\","
                "\"pl_avail\":\"connected\","
                "\"pl_not_avail\":\"disconnected\","
                "\"uniq_id\":\"#DEVICEID##SENSORID#\","
                "\"name\":\"#SENSORNAME#\","
                "\"ic\":\"mdi:gesture-tap-button\","
                "\"pl_on\":\"true\","
                "\"pl_off\":\"false\","
                "\"stat_t\":\"#MASTERTOPIC##STATETOPIC#\""
                "}"));
            configPayloadTemplate.replace(" ", "");
            configPayloadTemplate.replace(F("#DEVICEID#"), deviceID);
            configPayloadTemplate.replace(F("#HOSTNAME#"), hostname);
            configPayloadTemplate.replace(F("#VERSION#"), VERSION);
            if (mqttUseDeviceTopic)
            {
                configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttDeviceTopic);
            }
            else
            {
                configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttMasterTopic);
            }
            configPayloadTemplate.replace(F("#IP#"), ip_url);

            for (uint8_t n = 0; n < sizeof(btnEnabled) / sizeof(btnEnabled[0]); n++)
            {
                if (btnEnabled[n])
                {
                    topic = configTopicTemplate;
                    topic.replace(F("#COMPONENT#"), F("binary_sensor"));
                    topic.replace(F("#SENSORID#"), String(F("Button")) + String(n));

                    payload = configPayloadTemplate;
                    payload.replace(F("#SENSORID#"), String(F("Button")) + String(n));
                    payload.replace(F("#SENSORNAME#"), String(btnLogNames[n]));
                    payload.replace(F("#STATETOPIC#"), String(F("buttons/button")) + String(n));
                    client.publish(topic.c_str(), payload.c_str(), true);
                }
            }

            // Wifi RSSI
            configPayloadTemplate = String(F(
                "{"
                "\"dev\":{"
                "\"ids\":\"#DEVICEID#\","
                "\"name\":\"#HOSTNAME#\","
                "\"mdl\":\"PixelIt\","
                "\"mf\":\"PixelIt\","
                "\"sw\":\"#VERSION#\","
                "\"cu\":\"#IP#\""
                "},"
                "\"avty_t\":\"#MASTERTOPIC#state\","
                "\"pl_avail\":\"connected\","
                "\"pl_not_avail\":\"disconnected\","
                "\"uniq_id\":\"#DEVICEID##SENSORID#\","
                "\"dev_cla\":\"signal_strength\","
                "\"name\":\"#SENSORNAME#\","
                "\"stat_t\":\"#MASTERTOPIC##STATETOPIC#\","
                "\"unit_of_meas\":\"#UNIT#\","
                "\"val_tpl\":\"{{value_json.#VALUENAME#}}\","
                "\"ent_cat\":\"diagnostic\","
                "\"ic\":\"mdi:#ICON#\","
                "\"enabled_by_default\":\"false\""
                "}"));
            configPayloadTemplate.replace(" ", "");
            configPayloadTemplate.replace(F("#DEVICEID#"), deviceID);
            configPayloadTemplate.replace(F("#HOSTNAME#"), hostname);
            configPayloadTemplate.replace(F("#VERSION#"), VERSION);
            if (mqttUseDeviceTopic)
            {
                configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttDeviceTopic);
            }
            else
            {
                configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttMasterTopic);
            }
            configPayloadTemplate.replace(F("#IP#"), ip_url);

            topic = configTopicTemplate;
            topic.replace(F("#COMPONENT#"), F("sensor"));
            topic.replace(F("#SENSORID#"), F("wifiRSSI"));

            payload = configPayloadTemplate;
            payload.replace(F("#SENSORID#"), F("wifiRSSI"));
            payload.replace(F("#SENSORNAME#"), F("Wifi Signal"));
            payload.replace(F("#CLASS#"), F("signal_strength"));
            payload.replace(F("#STATETOPIC#"), F("matrixinfo"));
            payload.replace(F("#UNIT#"), "dBm");
            payload.replace(F("#VALUENAME#"), F("wifiRSSI"));
            payload.replace(F("#ICON#"), F("signal"));
            client.publish(topic.c_str(), payload.c_str(), true);

            // Wifi Quality sensor
            configPayloadTemplate = String(F(
                "{"
                "\"dev\":{"
                "\"ids\":\"#DEVICEID#\","
                "\"name\":\"#HOSTNAME#\","
                "\"mdl\":\"PixelIt\","
                "\"mf\":\"PixelIt\","
                "\"sw\":\"#VERSION#\","
                "\"cu\":\"#IP#\""
                "},"
                "\"avty_t\":\"#MASTERTOPIC#state\","
                "\"pl_avail\":\"connected\","
                "\"pl_not_avail\":\"disconnected\","
                "\"uniq_id\":\"#DEVICEID##SENSORID#\","
                "\"name\":\"#SENSORNAME#\","
                "\"stat_t\":\"#MASTERTOPIC##STATETOPIC#\","
                "\"unit_of_meas\":\"#UNIT#\","
                "\"val_tpl\":\"{{value_json.#VALUENAME#}}\","
                "\"ent_cat\":\"diagnostic\","
                "\"ic\":\"mdi:#ICON#\","
                "\"enabled_by_default\":\"false\""
                "}"));
            configPayloadTemplate.replace(" ", "");
            configPayloadTemplate.replace(F("#DEVICEID#"), deviceID);
            configPayloadTemplate.replace(F("#HOSTNAME#"), hostname);
            configPayloadTemplate.replace(F("#VERSION#"), VERSION);
            if (mqttUseDeviceTopic)
            {
                configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttDeviceTopic);
            }
            else
            {
                configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttMasterTopic);
            }
            configPayloadTemplate.replace(F("#IP#"), ip_url);

            topic = configTopicTemplate;
            topic.replace(F("#COMPONENT#"), F("sensor"));
            topic.replace(F("#SENSORID#"), F("WifiQuality"));

            payload = configPayloadTemplate;
            payload.replace(F("#SENSORID#"), F("WifiQuality"));
            payload.replace(F("#SENSORNAME#"), F("Wifi Quality"));
            payload.replace(F("#STATETOPIC#"), F("matrixinfo"));
            payload.replace(F("#UNIT#"), "%");
            payload.replace(F("#VALUENAME#"), F("wifiQuality"));
            payload.replace(F("#ICON#"), F("signal"));
            client.publish(topic.c_str(), payload.c_str(), true);

            // CPU Freq.
            topic = configTopicTemplate;
            topic.replace(F("#COMPONENT#"), F("sensor"));
            topic.replace(F("#SENSORID#"), F("cpuFreqMHz"));

            payload = configPayloadTemplate;
            payload.replace(F("#SENSORID#"), F("cpuFreqMHz"));
            payload.replace(F("#SENSORNAME#"), F("CPU Freq."));
            payload.replace(F("#STATETOPIC#"), F("matrixinfo"));
            payload.replace(F("#UNIT#"), "MHz");
            payload.replace(F("#VALUENAME#"), F("cpuFreqMHz"));
            payload.replace(F("#ICON#"), F("developer-board"));
            client.publish(topic.c_str(), payload.c_str(), true);

            // Wifi SSID
            configPayloadTemplate = String(F(
                "{"
                "\"dev\":{"
                "\"ids\":\"#DEVICEID#\","
                "\"name\":\"#HOSTNAME#\","
                "\"mdl\":\"PixelIt\","
                "\"mf\":\"PixelIt\","
                "\"sw\":\"#VERSION#\","
                "\"cu\":\"#IP#\""
                "},"
                "\"avty_t\":\"#MASTERTOPIC#state\","
                "\"pl_avail\":\"connected\","
                "\"pl_not_avail\":\"disconnected\","
                "\"uniq_id\":\"#DEVICEID##SENSORID#\","
                "\"name\":\"#SENSORNAME#\","
                "\"stat_t\":\"#MASTERTOPIC##STATETOPIC#\","
                "\"val_tpl\":\"{{value_json.#VALUENAME#}}\","
                "\"ent_cat\":\"diagnostic\","
                "\"ic\":\"mdi:#ICON#\","
                "\"enabled_by_default\":\"false\""
                "}"));
            configPayloadTemplate.replace(" ", "");
            configPayloadTemplate.replace(F("#DEVICEID#"), deviceID);
            configPayloadTemplate.replace(F("#HOSTNAME#"), hostname);
            configPayloadTemplate.replace(F("#VERSION#"), VERSION);
            if (mqttUseDeviceTopic)
            {
                configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttDeviceTopic);
            }
            else
            {
                configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttMasterTopic);
            }
            configPayloadTemplate.replace(F("#IP#"), ip_url);

            topic = configTopicTemplate;
            topic.replace(F("#COMPONENT#"), F("sensor"));
            topic.replace(F("#SENSORID#"), F("WifiSSID"));

            payload = configPayloadTemplate;
            payload.replace(F("#SENSORID#"), F("WifiSSID"));
            payload.replace(F("#SENSORNAME#"), F("SSID"));
            payload.replace(F("#STATETOPIC#"), F("matrixinfo"));
            payload.replace(F("#VALUENAME#"), F("wifiSSID"));
            payload.replace(F("#ICON#"), F("wifi"));
            client.publish(topic.c_str(), payload.c_str(), true);

            // Wifi BSSID
            topic = configTopicTemplate;
            topic.replace(F("#COMPONENT#"), F("sensor"));
            topic.replace(F("#SENSORID#"), F("WifiBSSID"));

            payload = configPayloadTemplate;
            payload.replace(F("#SENSORID#"), F("WifiBSSID"));
            payload.replace(F("#SENSORNAME#"), F("BSSID"));
            payload.replace(F("#STATETOPIC#"), F("matrixinfo"));
            payload.replace(F("#VALUENAME#"), F("wifiBSSID"));
            payload.replace(F("#ICON#"), F("wifi"));
            client.publish(topic.c_str(), payload.c_str(), true);

            // Chip ID
            topic = configTopicTemplate;
            topic.replace(F("#COMPONENT#"), F("sensor"));
            topic.replace(F("#SENSORID#"), F("chipID"));

            payload = configPayloadTemplate;
            payload.replace(F("#SENSORID#"), F("chipID"));
            payload.replace(F("#SENSORNAME#"), F("Chip ID"));
            payload.replace(F("#STATETOPIC#"), F("matrixinfo"));
            payload.replace(F("#VALUENAME#"), F("chipID"));
            payload.replace(F("#ICON#"), F("developer-board"));
            client.publish(topic.c_str(), payload.c_str(), true);

            // LED Matrix on/off + brightness light
            configPayloadTemplate = String(F(
                "{"
                "\"dev\":{"
                "\"ids\":\"#DEVICEID#\","
                "\"name\":\"#HOSTNAME#\","
                "\"mdl\":\"PixelIt\","
                "\"mf\":\"PixelIt\","
                "\"sw\":\"#VERSION#\","
                "\"cu\":\"#IP#\""
                "},"
                "\"avty_t\":\"#MASTERTOPIC#state\","
                "\"pl_avail\":\"connected\","
                "\"pl_not_avail\":\"disconnected\","
                "\"uniq_id\":\"#DEVICEID##SENSORID#\","
                "\"name\":\"#SENSORNAME#\","
                "\"schema\":\"template\","
                "\"stat_t\":\"#MASTERTOPIC##STATETOPIC#\","
                "\"stat_tpl\":\"{{ \'on\' if value_json.sleepMode is false else \'off\' }}\","
                "\"cmd_t\":\"#MASTERTOPIC##COMMANDTOPIC#\","
                "\"cmd_on_tpl\":\"{\\\"sleepMode\\\": false {%- if brightness is defined -%}, \\\"brightness\\\": {{ brightness }}{%- endif -%}}\","
                "\"cmd_off_tpl\":\"{\\\"sleepMode\\\": true}\","
                "\"bri_tpl\":\"{{ value_json.currentMatrixBrightness }}\","
                "\"icon\":\"mdi:#ICON#\""
                "}"));
            configPayloadTemplate.replace(F("#DEVICEID#"), deviceID);
            configPayloadTemplate.replace(F("#HOSTNAME#"), hostname);
            configPayloadTemplate.replace(F("#VERSION#"), VERSION);
            if (mqttUseDeviceTopic)
            {
                configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttDeviceTopic);
            }
            else
            {
                configPayloadTemplate.replace(F("#MASTERTOPIC#"), mqttMasterTopic);
            }
            configPayloadTemplate.replace(F("#IP#"), ip_url);

            topic = configTopicTemplate;
            topic.replace(F("#COMPONENT#"), F("light"));
            topic.replace(F("#SENSORID#"), F("LEDMatrixLight"));

            payload = configPayloadTemplate;
            payload.replace(F("#SENSORID#"), F("LEDMatrixLight"));
            payload.replace(F("#SENSORNAME#"), F("LED Matrix"));
            payload.replace(F("#STATETOPIC#"), F("matrixinfo"));
            payload.replace(F("#COMMANDTOPIC#"), F("setScreen"));
            payload.replace(F("#ICON#"), F("led-strip"));
            client.publish(topic.c_str(), payload.c_str(), true);

            Log(F("MQTTreconnect"), F("MQTT discovery information published"));
        }
    }
    else
    {
        Log(F("MQTTreconnect"), F("MQTT connect failed! Retry in a few seconds..."));
    }

    return connected;
}
/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
// Effekte

void FadeOut(int dealy, int minBrightness)
{
    int currentFadeBrightness = currentMatrixBrightness;

    int counter = 25;
    while (counter >= 0)
    {
        currentFadeBrightness = map(counter, 0, 25, minBrightness, currentMatrixBrightness);
        matrix->setBrightness(currentFadeBrightness);
        matrix->show();
        counter--;
        delay(dealy);
    }
}

void FadeIn(int dealy, int minBrightness)
{
    int currentFadeBrightness = minBrightness;

    int counter = 0;
    while (counter <= 25)
    {
        currentFadeBrightness = map(counter, 0, 25, minBrightness, currentMatrixBrightness);
        matrix->setBrightness(currentFadeBrightness);
        matrix->show();
        counter++;
        delay(dealy);
    }
}

void ColoredBarWipe()
{
    for (uint16_t i = 0; i < 32 + 1; i++)
    {
        matrix->fillRect(0, 0, i - 1, 8, matrix->Color(0, 0, 0));

        matrix->drawFastVLine(i, 0, 8, ColorWheel((i * 8) & 255, 0));
        matrix->drawFastVLine(i + 1, 0, 8, ColorWheel((i * 9) & 255, 0));
        matrix->drawFastVLine(i - 1, 0, 8, 0);
        matrix->drawFastVLine(i - 2, 0, 8, 0);
        matrix->show();
        delay(15);
    }
}

void ZigZagWipe(uint8_t r, uint8_t g, uint8_t b)
{
    for (uint16_t row = 0; row <= 7; row += 2)
    {
        for (uint16_t col = 0; col <= 31; col++)
        {
            if (row == 0 || row == 4)
            {
                matrix->fillRect(0, row, col - 1, 2, matrix->Color(0, 0, 0));
                matrix->drawFastVLine(col - 1, row, 2, matrix->Color(r, g, b));
                matrix->drawFastVLine(col, row, 2, matrix->Color(r, g, b));
            }
            else
            {
                matrix->fillRect(32 - col, row, col, 2, matrix->Color(0, 0, 0));
                matrix->drawFastVLine(32 - col, row, 2, matrix->Color(r, g, b));
                matrix->drawFastVLine(32 - col - 1, row, 2, matrix->Color(r, g, b));
            }
            matrix->show();
            delay(5);
        }
        matrix->fillRect(0, row, 32, 2, matrix->Color(0, 0, 0));
        if (row == 0 || row == 4)
        {
            matrix->drawFastVLine(30, row + 1, 2, matrix->Color(r, g, b));
            matrix->drawFastVLine(31, row + 1, 2, matrix->Color(r, g, b));
        }
        else
        {
            matrix->drawFastVLine(0, row + 1, 2, matrix->Color(r, g, b));
            matrix->drawFastVLine(1, row + 1, 2, matrix->Color(r, g, b));
        }
        matrix->show();
        delay(5);
        matrix->fillRect(0, row, 32, 2, matrix->Color(0, 0, 0));
    }
    matrix->fillRect(0, 0, 32, 8, matrix->Color(0, 0, 0));
    matrix->show();
}

void BitmapWipe(JsonArray &data, int16_t w)
{
    for (int16_t x = -w + 1; x <= 31; x++)
    {
        int16_t y = 0;
        for (int16_t j = 0; j < 8; j++, y++)
        {
            for (int16_t i = 0; i < w; i++)
            {
                matrix->drawPixel(x + i, y, data[j * w + i].as<uint16_t>());
            }
        }
        matrix->show();
        delay(18);
        matrix->fillRect(0, 0, x, 8, matrix->Color(0, 0, 0));
        matrix->show();
    }
}

void ColorFlash(int red, int green, int blue)
{
    for (uint16_t row = 0; row < 8; row++)
    {
        for (uint16_t column = 0; column < 32; column++)
        {
            matrix->drawPixel(column, row, matrix->Color(red, green, blue));
        }
    }
    matrix->show();
}

uint ColorWheel(byte wheelPos, int pos)
{
    if (wheelPos < 85)
    {
        return matrix->Color((wheelPos * 3) - pos, (255 - wheelPos * 3) - pos, 0);
    }
    else if (wheelPos < 170)
    {
        wheelPos -= 85;
        return matrix->Color((255 - wheelPos * 3) - pos, 0, (wheelPos * 3) - pos);
    }
    else
    {
        wheelPos -= 170;
        return matrix->Color(0, (wheelPos * 3) - pos, (255 - wheelPos * 3) - pos);
    }
}

void ShowBootAnimation()
{
    DrawTextHelper("P", false, false, false, false, false, 255, 51, 255, (MATRIX_WIDTH / 2) - 12, 1);
    matrix->show();

    delay(200);
    DrawTextHelper("I", false, false, false, false, false, 0, 255, 42, (MATRIX_WIDTH / 2) - 8, 1);
    matrix->show();

    delay(200);
    DrawTextHelper("X", false, false, false, false, false, 255, 25, 25, (MATRIX_WIDTH / 2) - 6, 1);
    matrix->show();

    delay(200);
    DrawTextHelper("E", false, false, false, false, false, 25, 255, 255, (MATRIX_WIDTH / 2) - 2, 1);
    matrix->show();

    delay(200);
    DrawTextHelper("L", false, false, false, false, false, 255, 221, 51, (MATRIX_WIDTH / 2) + 2, 1);
    matrix->show();

    delay(500);
    DrawTextHelper("I", false, false, false, false, false, 255, 255, 255, (MATRIX_WIDTH / 2) + 6, 1);
    DrawTextHelper("T", false, false, false, false, false, 255, 255, 255, (MATRIX_WIDTH / 2) + 8, 1);
    matrix->show();
    delay(1000);
}

void ShowBatteryScreen()
{
    const size_t capacity = JSON_ARRAY_SIZE(64) + JSON_OBJECT_SIZE(1) + 2 * JSON_OBJECT_SIZE(2) + JSON_OBJECT_SIZE(3) + 350;
    DynamicJsonBuffer jsonBuffer(capacity);
    const char *json = "{\"bitmap\":{\"data\":[0,0,65535,65535,65535,0,0,0,0,0,65535,2016,65535,0,0,0,0,65535,2016,2016,2016,65535,0,0,0,65535,2016,2016,2016,65535,0,0,0,65535,2016,2016,2016,65535,0,0,0,65535,2016,2016,2016,65535,0,0,0,65535,2016,2016,2016,65535,0,0,0,65535,65535,65535,65535,65535,0,0],\"position\":{\"x\":0,\"y\":0},\"size\":{\"width\":8,\"height\":8}}}";
    JsonObject &root = jsonBuffer.parseObject(json);
    if (root.success())
    {
        Serial.println("parsed json");
    }
    else
    {
        Serial.println("failed to parse json");
    }
    getBatteryVoltage();
    matrix->clear();
    DrawSingleBitmap(root["bitmap"]);
    DrawTextHelper(String(batteryLevel, 0) + "%", false, true, false, false, false, 255, 255, 255, 9, 1);
    matrix->show();
    delay(1000);
}

ColorTemperature GetUserColorTemp()
{
    if (matrixTempCorrection == "tungsten40w")
    {
        return Tungsten40W;
    }

    if (matrixTempCorrection == "tungsten100w")
    {
        return Tungsten100W;
    }

    if (matrixTempCorrection == "halogen")
    {
        return Halogen;
    }

    if (matrixTempCorrection == "carbonarc")
    {
        return CarbonArc;
    }

    if (matrixTempCorrection == "highnoonsun")
    {
        return HighNoonSun;
    }

    if (matrixTempCorrection == "directsunlight")
    {
        return DirectSunlight;
    }

    if (matrixTempCorrection == "overcastsky")
    {
        return OvercastSky;
    }

    if (matrixTempCorrection == "clearbluesky")
    {
        return ClearBlueSky;
    }

    if (matrixTempCorrection == "warmfluorescent")
    {
        return WarmFluorescent;
    }

    if (matrixTempCorrection == "standardfluorescent")
    {
        return StandardFluorescent;
    }

    if (matrixTempCorrection == "coolwhitefluorescent")
    {
        return CoolWhiteFluorescent;
    }
    if (matrixTempCorrection == "fullspectrumfluorescent")
    {
        return FullSpectrumFluorescent;
    }
    if (matrixTempCorrection == "growlightfluorescent")
    {
        return GrowLightFluorescent;
    }
    if (matrixTempCorrection == "blacklightfluorescent")
    {
        return BlackLightFluorescent;
    }
    if (matrixTempCorrection == "mercuryvapor")
    {
        return MercuryVapor;
    }
    if (matrixTempCorrection == "sodiumvapor")
    {
        return SodiumVapor;
    }
    if (matrixTempCorrection == "metalhalide")
    {
        return MetalHalide;
    }
    if (matrixTempCorrection == "highpressuresodium")
    {
        return HighPressureSodium;
    }

    return UncorrectedTemperature;
}

LEDColorCorrection GetUserColorCorrection()
{
    if (matrixTempCorrection == "default")
    {
        return TypicalSMD5050;
    }

    if (matrixTempCorrection == "typicalsmd5050")
    {
        return TypicalSMD5050;
    }

    if (matrixTempCorrection == "typical8mmpixel")
    {
        return Typical8mmPixel;
    }

    return UncorrectedColor;
}

int *GetUserCutomCorrection()
{
    String rgbString = matrixTempCorrection;
    rgbString.trim();

    // R,G,B / 255,255,255
    static int rgbArray[3];

    // R
    rgbArray[0] = rgbString.substring(0, 3).toInt();
    // G
    rgbArray[1] = rgbString.substring(4, 7).toInt();
    // B
    rgbArray[2] = rgbString.substring(8, 11).toInt();

    return rgbArray;
}

LightDependentResistor::ePhotoCellKind TranslatePhotocell(String photocell)
{
    if (photocell == "GL5516")
        return LightDependentResistor::GL5516;
    if (photocell == "GL5528")
        return LightDependentResistor::GL5528;
    if (photocell == "GL5537_1")
        return LightDependentResistor::GL5537_1;
    if (photocell == "GL5537_2")
        return LightDependentResistor::GL5537_2;
    if (photocell == "GL5539")
        return LightDependentResistor::GL5539;
    if (photocell == "GL5549")
        return LightDependentResistor::GL5549;
    Log(F("LDR assignment - unknown type"), photocell);
    return LightDependentResistor::GL5528;
}

uint8_t TranslatePin(String pin)
{
#if defined(ESP8266)
    if (pin == "Pin_D0")
        return D0;
    if (pin == "Pin_D1")
        return D1;
    if (pin == "Pin_D2")
        return D2;
    if (pin == "Pin_D3")
        return D3;
    if (pin == "Pin_D4")
        return D4;
    if (pin == "Pin_D5")
        return D5;
    if (pin == "Pin_D6")
        return D6;
    if (pin == "Pin_D7")
        return D7;
    if (pin == "Pin_D8")
        return D8;
    if (pin == "Pin_27")
        return 27;
    Log(F("Pin assignment - unknown pin"), pin);
    return LED_BUILTIN;
#elif defined(ESP32)

    if (pin == "GPIO_NUM_14")
        return GPIO_NUM_14;
    if (pin == "GPIO_NUM_15")
        return GPIO_NUM_15;
    if (pin == "GPIO_NUM_16")
        return GPIO_NUM_16;
    if (pin == "GPIO_NUM_17")
        return GPIO_NUM_17;
    if (pin == "GPIO_NUM_18")
        return GPIO_NUM_18;
    if (pin == "GPIO_NUM_19")
        return GPIO_NUM_19;
    if (pin == "GPIO_NUM_21")
        return GPIO_NUM_21;
    if (pin == "GPIO_NUM_22")
        return GPIO_NUM_22;
    if (pin == "GPIO_NUM_23")
        return GPIO_NUM_23;
    if (pin == "GPIO_NUM_25")
        return GPIO_NUM_25;
    if (pin == "GPIO_NUM_26")
        return GPIO_NUM_26;
    if (pin == "GPIO_NUM_27")
        return GPIO_NUM_27;
    if (pin == "SPI_CLK_GPIO_NUM")
        return SPI_CLK_GPIO_NUM;
    if (pin == "SPI_CS0_GPIO_NUM")
        return SPI_CS0_GPIO_NUM;

    Log(F("Pin assignment - unknown pin"), pin);
    return GPIO_NUM_32; // IDK
#endif
}

void ClearTextArea()
{
    int16_t h = 8;
    int16_t w = 24;
    int16_t x = 8;
    int16_t y = 0;

    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            matrix->drawPixel(x + i, y, (uint16_t)0);
        }
    }
}

void ClearBMPArea()
{
    int16_t h = 8;
    int16_t w = 8;
    int16_t x = 0;
    int16_t y = 0;

    for (int16_t j = 0; j < h; j++, y++)
    {
        for (int16_t i = 0; i < w; i++)
        {
            matrix->drawPixel(x + i, y, (uint16_t)0);
        }
    }
}

int DayOfWeekFirstMonday(int OrigDayofWeek)
{
    int idx = (7 + OrigDayofWeek) - 1;
    if (idx > 6) // week ends at 6, because Enum.DayOfWeek is zero-based
    {
        idx -= 7;
    }
    return idx;
    // int diff = (7 + (OrigDayofWeek - 1)) % 7;
    // return OrigDayofWeek + (-1 * diff);
}

void initDFPlayer()
{
    if (!mp3Player.begin(*softSerial))
    {
        Log(F("DFPlayer"), F("DFPlayer not found"));
    }
    else
    {
        Log(F("DFPlayer"), F("DFPlayer started"));
        mp3Player.volume(initialVolume);
    }
}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
void setup()
{
#if defined(ULANZI)
    pinMode(15, INPUT_PULLDOWN); // Fix high pitch tone
    pinMode(27, INPUT_PULLUP);   // Middle Button fix
    pinMode(26, INPUT_PULLUP);   // Left Button fix
    pinMode(VBAT_PIN, INPUT);    // Battery ADC
#endif

    Serial.begin(115200);

    // Mounting FileSystem
    Serial.println(F("Mounting file system..."));
#if defined(ESP8266)
    if (LittleFS.begin())
#elif defined(ESP32)
    if (SPIFFS.begin(true))
#endif
    {
        Serial.println(F("Mounted file system."));
        LoadConfig();
        // If new version detected, create new variables in config if necessary.
        if (optionsVersion != VERSION)
        {
            Log(F("LoadConfig"), F("New version detected, create new variables in config if necessary"));
            SaveConfig();
            LoadConfig();
        }
    }
    else
    {
        Serial.println(F("Failed to mount FS"));
    }

    // Init SetGPIO Array
    for (int i = 0; i < SET_GPIO_SIZE; i++)
    {
        setGPIOReset[i].gpio = -1;
        setGPIOReset[i].resetMillis = -1;
    }

    // I2C Sensors
    twowire.begin(TranslatePin(SDAPin), TranslatePin(SCLPin));

    // Init LightSensor
    bh1750 = new BH1750();
    if (bh1750->begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23, &twowire))
    {
        Log(F("Setup"), F("BH1750 started"));
        luxSensor = LuxSensor_BH1750;
    }
    else
    {
        delete bh1750;
        max44009 = new Max44009(Max44009::Boolean::False);
        max44009->configure(MAX44009_DEFAULT_ADDRESS, &twowire, Max44009::Boolean::False);
        if (max44009->isConnected())
        {
            Log(F("Setup"), F("Max44009/GY-049 started"));
            luxSensor = LuxSensor_Max44009;
        }
        else
        {
            delete max44009;
            photocell = new LightDependentResistor(LDR_PIN, ldrPulldown, TranslatePhotocell(ldrDevice), 10, ldrSmoothing);
            photocell->setPhotocellPositionOnGround(false);
            luxSensor = LuxSensor_LDR;
        }
    }

    // Init Temp Sensors
    Log(F("Setup"), F("SHT31 Trying"));
    if (sht31.begin(0x44))
    {
        Log(F("Setup"), F("SHT31 started"));
        tempSensor = TempSensor_SHT31;
    }
    else
    {
        Log(F("Setup"), F("BME280 Trying"));
        bme280 = new Adafruit_BME280();
        if (bme280->begin(BME280_ADDRESS_ALTERNATE, &twowire))
        {
            Log(F("Setup"), F("BME280 started"));
            tempSensor = TempSensor_BME280;
        }
        else
        {
            delete bme280;
            bmp280 = new Adafruit_BMP280(&twowire);
            Log(F("Setup"), F("BMP280 Trying"));
            if (bmp280->begin(BMP280_ADDRESS_ALT, 0x58))
            {
                Log(F("Setup"), F("BMP280 started"));
                tempSensor = TempSensor_BMP280;
            }
            else
            {
                delete bmp280;
                bme680 = new Adafruit_BME680(&twowire);
                Log(F("Setup"), F("BME680 Trying"));
                if (bme680->begin())
                {
                    Log(F("Setup"), F("BME680 started"));
                    tempSensor = TempSensor_BME680;
                }
                else
                {
                    Log(F("Setup"), F("No SHT31, BMP280, BME280 or BME680 sensor found"));
                    // AM2320 needs a delay to be reliably initialized
                    delete bme680;

                    // continue only if:
                    //  - LDR is being used. This means: no light sensor in I²C bus.
                    //  - SDA and SCL use different pin than onewire

                    // Otherwise, we already found a light sensor on I²C. If we would start a probe for OneWire on the same pin now, I²C will be disfunctional.
                    if (luxSensor == LuxSensor_LDR || (onewirePin != SDAPin && onewirePin != SCLPin))
                    {
                        delay(800);
                        dht.setup(TranslatePin(onewirePin), DHTesp::DHT22);
                        Log(F("Setup"), F("DHT Trying"));
                        if (!isnan(dht.getHumidity()) && !isnan(dht.getTemperature()))
                        {
                            Log(F("Setup"), F("DHT started"));
                            tempSensor = TempSensor_DHT;
                        }
                        else
                        {
                            Log(F("Setup"), F("No DHT Sensor found"));
                        }
                    }
                    else
                    {
                        Log(F("Setup"), F("Not probing DHT sensor: light sensor already found on same pin as DHT."));
                    }
                }
            }
        }
    }

    switch (matrixType)
    {
    default: // Matix Type 1 (Colum major)
        matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG);
        break;

    case 2: // Matix Type 2 (Row major)
        matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG);
        break;

    case 3: // Matix Type 3 (Tiled 4x 8x8 CJMCU, Column major)
        matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_BOTTOM + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS + NEO_MATRIX_PROGRESSIVE);
        break;

    case 4: // Matix Type 4 (MicroMatrix by foorschtbar) See: https://github.com/foorschtbar/Sk6805EC15-Matrix
        matrix = new FastLED_NeoMatrix(leds, 8, 8, 4, 1, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG);
        break;

    case 5: // Matix Type 5 (Tiled 4x 8x8 CJMCU, Row major)
        matrix = new FastLED_NeoMatrix(leds, 8, 8, 4, 1, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_PROGRESSIVE);
        break;
    }

    ColorTemperature userColorTemp = GetUserColorTemp();
    LEDColorCorrection userLEDCorrection = GetUserColorCorrection();

    // Matrix Color Correction
    if (userLEDCorrection != UncorrectedColor)
    {
        FastLED.addLeds<NEOPIXEL, MATRIX_PIN>(leds, MATRIX_WIDTH * MATRIX_HEIGHT).setCorrection(userLEDCorrection);
    }
    else if (userColorTemp != UncorrectedTemperature)
    {
        FastLED.addLeds<NEOPIXEL, MATRIX_PIN>(leds, MATRIX_WIDTH * MATRIX_HEIGHT).setTemperature(userColorTemp);
    }
    else
    {
        int *rgbArray = GetUserCutomCorrection();
        FastLED.addLeds<NEOPIXEL, MATRIX_PIN>(leds, MATRIX_WIDTH * MATRIX_HEIGHT).setCorrection(matrix->Color(rgbArray[0], rgbArray[1], rgbArray[2]));
    }

    matrix->begin();
    matrix->setTextWrap(false);
    matrix->setBrightness(currentMatrixBrightness);
    matrix->clear();

    softSerial = new SoftwareSerial(TranslatePin(dfpTXPin), TranslatePin(dfpRXPin));

    softSerial->begin(9600);
    Log(F("Setup"), F("Software Serial started"));

    // Play sound on boot
    if (bootSound)
    {
        delay(1000); // is needed for the dfplayer to startup
        initDFPlayer();
        delay(10);
        mp3Player.play(1);
    }

    // Bootscreen
    if (bootScreenAktiv)
    {
        ShowBootAnimation();
    }

    // Battery
    if (bootBatteryScreen)
    {
        ShowBatteryScreen();
    }

    // Set unique device ID
    deviceID = "PixelIt-";
#if defined(ESP8266)
    deviceID += ESP.getChipId();
#elif defined(ESP32)
    deviceID += uint64ToString(ESP.getEfuseMac());
#endif
    // Set hostname from config
    // variable is already validated in LoadConfig()
    if (hostname.isEmpty())
    {
        hostname = deviceID;
    }
    WiFi.hostname(hostname);
    mqttDeviceTopic = mqttMasterTopic + hostname + "/";

    wifiManager.setAPCallback(EnteredHotspotCallback);
    wifiManager.setMinimumSignalQuality();
    // Timout for the wifi connection until the hotspot is set up
    wifiManager.setTimeout(30);
    // Config menue timeout 180 seconds.
    wifiManager.setConfigPortalTimeout(180);

    if (!wifiManager.autoConnect("PIXELIT"))
    {
        Log(F("Setup"), F("Wifi failed to connect and hit timeout"));
        delay(3000);
        // Reset and try again, or maybe put it to deep sleep
        ESP.restart();
        delay(5000);
    }

    Log(F("Setup"), F("Wifi connected...yeey :)"));

    Log(F("Setup"), F("Local IP"));
    Log(F("Setup"), WiFi.localIP().toString());
    Log(F("Setup"), WiFi.gatewayIP().toString());
    Log(F("Setup"), WiFi.subnetMask().toString());

    Log(F("Setup"), F("Starting UDP"));
    udp.begin(2390);
    // Log(F("Setup"), "Local port: " + String(udp.localPort()));

    httpUpdater.setup(&server);

    server.on(F("/api/screen"), HTTP_POST, HandleScreen);
    server.on(F("/api/luxsensor"), HTTP_GET, HandleGetLuxSensor);
    server.on(F("/api/brightness"), HTTP_GET, HandleGetBrightness);
    server.on(F("/api/dhtsensor"), HTTP_GET, HandleGetDHTSensor); // Legacy
    server.on(F("/api/sensor"), HTTP_GET, HandleGetSensor);
    server.on(F("/api/buttons"), HTTP_GET, HandleGetButtons);
    server.on(F("/api/matrixinfo"), HTTP_GET, HandleGetMatrixInfo);
    // server.on(F("/api/soundinfo"), HTTP_GET, HandleGetSoundInfo);
    server.on(F("/api/config"), HTTP_POST, HandleSetConfig);
    server.on(F("/api/config"), HTTP_GET, HandleGetConfig);
    server.on(F("/api/wifireset"), HTTP_POST, HandelWifiConfigReset);
    server.on(F("/api/factoryreset"), HTTP_POST, HandleFactoryReset);
    server.on(F("/"), HTTP_GET, HandleGetMainPage);
    server.onNotFound(HandleNotFound);

    server.begin();

    webSocket.begin();
    webSocket.onEvent(webSocketEvent);

    // Liveview
    liveview.begin(matrix, leds, SEND_LIVEVIEW_INTERVAL); // pass pointer to matrix, ledbuffer and interval
    liveview.setCallback(sendLiveview);                   // set callback function which is called after the interval

    Log(F("Setup"), F("Webserver started"));

    if (mqttAktiv == true)
    {
        client.setServer(mqttServer.c_str(), mqttPort);
        client.setCallback(callback);
        client.setBufferSize(8000);
        Log(F("Setup"), F("MQTT started"));
    }

    if (!bootSound)
    {
        initDFPlayer();
    }
}

void displayUpdateScreen()
{
    Log(F("UpdateScreen"), F("Display UpdateScreen..."));

    DynamicJsonBuffer jsonBuffer;
    JsonObject &root = jsonBuffer.createObject();

    BuildUpdateScreenJSON(root);

    if (root.success())
    {
        CreateFrames(root, CHECKUPDATESCREEN_DURATION);
    }
    else
    {
        Log(F("UpdateScreen"), F("Failed to Build UpdateScreen JSON"));
    }
}

void checkUpdate()
{
    Log(F("CheckUpdate"), F("Checking..."));
    HttpClient httpClient = HttpClient(wifiClientHTTP, CHECKUPDATE_SERVER_HOST, CHECKUPDATE_SERVER_PORT);
    httpClient.sendHeader("User-Agent", "PixelIt");
    httpClient.setTimeout(1500);
    httpClient.get(CHECKUPDATE_SERVER_PATH);
    int statusCode = httpClient.responseStatusCode();
    String response = httpClient.responseBody();

    // Serial.print("Status code: ");
    // Serial.println(statusCode);
    // Serial.print("Response: ");
    // Serial.println(response);

    if (statusCode == 200)
    {
        DynamicJsonBuffer jsonBuffer;
        JsonObject &root = jsonBuffer.parseObject(response);
        if (root.containsKey("version"))
        {
            lastReleaseVersion = root["version"].as<String>();

            int result = compareVersions(lastReleaseVersion.c_str(), VERSION);

            if (result > 0)
            {
                Log(F("CheckUpdate"), "New FW available " + String(VERSION) + " -> " + lastReleaseVersion);
            }
            else
            {
                Log(F("CheckUpdate"), "No new FW available " + String(VERSION) + " -> " + lastReleaseVersion);
            }
        }
    }
    else
    {
        Log(F("CheckUpdate"), "Error. HTTP Code: " + statusCode);
    }
}

void loop()
{
    server.handleClient();
    webSocket.loop();

    // Update Battery level
    if (millis() - batteryLevelPrevMillis >= UPDATE_BATTERY_LEVEL_INTERVAL)
    {
        batteryLevelPrevMillis = millis();
        getBatteryVoltage();
    }

    // Reset GPIO based on the array, as far as something is present in the array.
    for (int i = 0; i < SET_GPIO_SIZE; i++)
    {
        if (setGPIOReset[i].gpio != -1)
        {
            if (setGPIOReset[i].resetMillis <= millis())
            {
                Log(F("ResetSetGPIO"), "Pos: " + String(i) + ", GPIO: " + String(setGPIOReset[i].gpio) + ", Value: false");
                digitalWrite(setGPIOReset[i].gpio, false);
                setGPIOReset[i].gpio = -1;
                setGPIOReset[i].resetMillis = -1;
            }
        }
    }

    // Check and display if new FW version is available.
    // if necessary also check scrollTextAktivLoop = false; and animateBMPAktivLoop = false; if they are disturbed?!
    if (checkUpdateScreen == true)
    {

        // Check new FW Version first time after 30.5 seconds
        if ((checkUpdatePrevMillis == 0 && millis() > 30500) || millis() - checkUpdatePrevMillis >= CHECKUPDATE_INTERVAL)
        {
            checkUpdatePrevMillis = millis();
            checkUpdate();
        }

        // Display new FW Version
        if (millis() - checkUpdateScreenPrevMillis >= CHECKUPDATESCREEN_INTERVAL)
        {
            checkUpdateScreenPrevMillis = millis();

            int result = compareVersions(lastReleaseVersion.c_str(), VERSION);
            if (result > 0)
            {
                if (!sleepMode)
                {
                    displayUpdateScreen();
                }
            }
        }
    }

    // Send Telemetry data first time after 30.3 seconds
    // if necessary also check scrollTextAktivLoop = false; and animateBMPAktivLoop = false; if they are disturbed?!
    if (sendTelemetry == true && ((sendTelemetryPrevMillis == 0 && millis() > 30300) || millis() - sendTelemetryPrevMillis >= SEND_TELEMETRY_INTERVAL))
    {
        sendTelemetryPrevMillis = millis();
        SendTelemetry();
    }

    if (mqttAktiv == true)
    {
        if (!client.connected())
        {
            // MQTT connect
            if (mqttLastReconnectAttempt == 0 || (millis() - mqttLastReconnectAttempt) >= MQTT_RECONNECT_INTERVAL)
            {
                mqttLastReconnectAttempt = millis();

                // try to reconnect
                if (MQTTreconnect())
                {
                    mqttLastReconnectAttempt = 0;
                }
            }
        }
        else
        {
            client.loop();
        }
    }

    // Check buttons
    for (uint button = 0; button < 3; button++)
    {
        if (btnEnabled[button])
        {
            if ((btnState[button] == btnState_Released) && (digitalRead(TranslatePin(btnPin[button])) == btnPressedLevel[button]))
            {
                btnState[button] = btnState_PressedNew;
            }
            if ((btnState[button] == btnState_PressedBefore) && (digitalRead(TranslatePin(btnPin[button])) != btnPressedLevel[button]))
            {
                btnState[button] = btnState_Released;
                HandleAndSendButtonPress(button, false);
            }
            if (btnState[button] == btnState_PressedNew)
            {
                btnState[button] = btnState_PressedBefore;
                HandleAndSendButtonPress(button, true);
            }
        }
    }

    // Clock Auto Fallback
    if (!sleepMode && ((clockAutoFallbackActive && !clockAktiv && millis() - lastScreenMessageMillis >= (clockAutoFallbackTime * 1000)) || forceClock))
    {
        forceClock = false;
        scrollTextAktivLoop = false;
        animateBMPAktivLoop = false;

        int performWipe = 0;

        switch (clockAutoFallbackAnimation)
        {
        case 1:
        case 2:
        case 3:
            performWipe = clockAutoFallbackAnimation;
            break;
        case 4:
            performWipe = (millis() % 3) + 1;
            break;
        default:;
        }

        if (performWipe == 1)
        {
            FadeOut();
        }
        else if (performWipe == 2)
        {
            ColoredBarWipe();
        }
        else if (performWipe == 3)
        {
            ZigZagWipe(clockColorR, clockColorG, clockColorB);
        }
        clockAktiv = true;
        clockCounterClock = 0;
        clockCounterDate = 0;
        DrawClock(true);

        if (performWipe != 0)
        {
            FadeIn();
        }
    }

    if (clockAktiv && now() != clockLastUpdate)
    {
        if (timeStatus() == timeNotSet && ntpTimeOut <= millis())
        {
            if (ntpRetryCounter >= NTP_MAX_RETRYS)
            {
                ntpTimeOut = (millis() + (NTP_TIMEOUT_SEC * 1000));
                ntpRetryCounter = 0;
                Log(F("Sync TimeServer"), "sync timeout for " + String(NTP_TIMEOUT_SEC) + " seconds!");
            }
            else
            {
                Log(F("Sync TimeServer"), ntpServer + " waiting for sync");
                setSyncProvider(getNtpTime);
            }
        }
        clockLastUpdate = now();
        DrawClock(false);
    }

    // Get Lunx and control brightness
    if (millis() - getLuxPrevMillis >= SEND_LUX_INTERVAL)
    {
        getLuxPrevMillis = millis();

        if (luxSensor == LuxSensor_BH1750)
        {
            currentLux = bh1750->readLightLevel() + luxOffset;
        }
        else if (luxSensor == LuxSensor_Max44009)
        {
            currentLux = max44009->getLux() + luxOffset;
        }
        else
        {
            currentLux = (roundf(photocell->getSmoothedLux() * 1000) / 1000) + luxOffset;
        }

        if (!sleepMode && matrixBrightnessAutomatic)
        {
            float newBrightness = map(currentLux, mbaLuxMin, mbaLuxMax, mbaDimMin, mbaDimMax);
            // Max brightness 255
            if (newBrightness > 255)
            {
                newBrightness = 255;
            }
            // Min brightness 0
            if (newBrightness < 0)
            {
                newBrightness = 0;
            }

            if (newBrightness != currentMatrixBrightness)
            {
                SetCurrentMatrixBrightness(newBrightness);
                Log(F("Auto Brightness"), "Lux: " + String(currentLux) + " set brightness to " + String(currentMatrixBrightness));
                matrix->show();
            }
        }
    }

    // Send LDR values non-foreced
    if (millis() - sendLuxPrevMillis >= SEND_LUX_INTERVAL)
    {
        sendLuxPrevMillis = millis();
        SendLDR(false);
    }

    // Send Sensor values non-foreced
    if (millis() - sendSensorPrevMillis >= SEND_SENSOR_INTERVAL)
    {
        sendSensorPrevMillis = millis();
        SendSensor(false);
    }

    // liveview
    liveview.loop();

    // send matrix info
    if (millis() - sendInfoPrevMillis >= SEND_MATRIXINFO_INTERVAL)
    {
        sendInfoPrevMillis = millis();
        SendMatrixInfo();
        // SendMp3PlayerInfo(false);
    }

    if (!sleepMode && (animateBMPAktivLoop && millis() - animateBMPPrevMillis >= animateBMPDelay))
    {
        animateBMPPrevMillis = millis();
        AnimateBMP(true);
    }

    if (!sleepMode && (scrollTextAktivLoop && millis() - scrollTextPrevMillis >= scrollTextDelay))
    {
        scrollTextPrevMillis = millis();
        ScrollText(false);
    }
}

void SendMatrixInfo()
{
    // Check if mqtt or websocket connected
    if ((mqttAktiv == true && client.connected()) || (webSocket.connectedClients() > 0))
    {
        String matrixInfo = GetMatrixInfo();

        // Check if sending via MQTT is required
        if (mqttAktiv == true && client.connected())
        {
            client.publish((mqttMasterTopic + "matrixinfo").c_str(), matrixInfo.c_str(), true);
            if (mqttUseDeviceTopic)
            {
                client.publish((mqttDeviceTopic + "matrixinfo").c_str(), matrixInfo.c_str(), true);
            }
        }
        // Check if sending via websocket is required
        if (webSocket.connectedClients() > 0)
        {
            for (uint i = 0; i < sizeof websocketConnection / sizeof websocketConnection[0]; i++)
            {
                webSocket.sendTXT(i, "{\"sysinfo\":" + matrixInfo + "}");
            }
        }
    }
}

void SendLDR(bool force)
{
    if (force)
    {
        oldGetLuxSensor = "";
    }

    String luxSensor;

    // Prüfen ob die Abfrage des LuxSensor überhaupt erforderlich ist
    if ((mqttAktiv == true && client.connected()) || (webSocket.connectedClients() > 0))
    {
        luxSensor = GetLuxSensor();
    }
    // Prüfen ob über MQTT versendet werden muss
    if (mqttAktiv == true && client.connected() && oldGetLuxSensor != luxSensor)
    {
        client.publish((mqttMasterTopic + "luxsensor").c_str(), luxSensor.c_str(), true);
        if (mqttUseDeviceTopic)
        {
            client.publish((mqttDeviceTopic + "luxsensor").c_str(), luxSensor.c_str(), true);
        }
    }
    // Prüfen ob über Websocket versendet werden muss
    if (webSocket.connectedClients() > 0 && oldGetLuxSensor != luxSensor)
    {
        for (unsigned int i = 0; i < sizeof websocketConnection / sizeof websocketConnection[0]; i++)
        {
            webSocket.sendTXT(i, "{\"sensor\":" + luxSensor + "}");
        }
    }

    oldGetLuxSensor = luxSensor;
}
void sendLiveview(const char *data, size_t length)
{
    if (webSocket.connectedClients() > 0)
    {
        for (unsigned int i = 0; i < sizeof websocketConnection / sizeof websocketConnection[0]; i++)
        {
            webSocket.sendTXT(i, data, length);
        }
    }
}

void SendSensor(bool force)
{
    if (force)
    {
        oldGetSensor = "";
    }

    String Sensor;

    // Prüfen ob die Abfrage des LuxSensor überhaupt erforderlich ist
    if ((mqttAktiv == true && client.connected()) || (webSocket.connectedClients() > 0))
    {
        Sensor = GetSensor();
    }
    // Prüfen ob über MQTT versendet werden muss
    if (mqttAktiv == true && client.connected() && oldGetSensor != Sensor)
    {
        client.publish((mqttMasterTopic + "dhtsensor").c_str(), Sensor.c_str(), true); // Legancy
        client.publish((mqttMasterTopic + "sensor").c_str(), Sensor.c_str(), true);
        if (mqttUseDeviceTopic)
        {
            client.publish((mqttDeviceTopic + "dhtsensor").c_str(), Sensor.c_str(), true); // Legancy
            client.publish((mqttDeviceTopic + "sensor").c_str(), Sensor.c_str(), true);
        }
    }
    // Prüfen ob über Websocket versendet werden muss
    if (webSocket.connectedClients() > 0 && oldGetSensor != Sensor)
    {
        for (uint i = 0; i < sizeof websocketConnection / sizeof websocketConnection[0]; i++)
        {
            webSocket.sendTXT(i, "{\"sensor\":" + Sensor + "}");
        }
    }

    oldGetSensor = Sensor;
}

void SendConfig()
{
    if (webSocket.connectedClients() > 0)
    {
        for (uint i = 0; i < sizeof websocketConnection / sizeof websocketConnection[0]; i++)
        {
            String config = GetConfig();
            webSocket.sendTXT(i, "{\"config\":" + config + "}");
        }
    }
}

/////////////////////////////////////////////////////////////////////
/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

time_t getNtpTime()
{
    while (udp.parsePacket() > 0)
        ;
    sendNTPpacket(ntpServer);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500)
    {
        int size = udp.parsePacket();
        if (size >= NTP_PACKET_SIZE)
        {
            udp.read(packetBuffer, NTP_PACKET_SIZE);
            time_t secsSince1900;

            secsSince1900 = (time_t)packetBuffer[40] << 24;
            secsSince1900 |= (time_t)packetBuffer[41] << 16;
            secsSince1900 |= (time_t)packetBuffer[42] << 8;
            secsSince1900 |= (time_t)packetBuffer[43];
            time_t secsSince1970 = secsSince1900 - 2208988800UL;
            float totalOffset = clockTimeZone;
            if (clockDayLightSaving)
            {
                totalOffset = (clockTimeZone + DSToffset(secsSince1970, clockTimeZone));
            }
            return secsSince1970 + (time_t)(totalOffset * SECS_PER_HOUR);
            ntpRetryCounter = 0;
        }
        yield();
    }
    ntpRetryCounter++;
    return 0;
}
void sendNTPpacket(String &address)
{
    memset(packetBuffer, 0, NTP_PACKET_SIZE);

    packetBuffer[0] = 0b11100011;
    packetBuffer[1] = 0;
    packetBuffer[2] = 6;
    packetBuffer[3] = 0xEC;

    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;

    udp.beginPacket(address.c_str(), 123);
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();
}

void Log(String function, String message)
{

    String timeStamp = IntFormat(year()) + "-" + IntFormat(month()) + "-" + IntFormat(day()) + "T" + IntFormat(hour()) + ":" + IntFormat(minute()) + ":" + IntFormat(second());

    Serial.println("[" + timeStamp + "] " + function + ": " + message);

    // Prüfen ob über Websocket versendet werden muss
    if (webSocket.connectedClients() > 0)
    {
        for (unsigned int i = 0; i < sizeof websocketConnection / sizeof websocketConnection[0]; i++)
        {
            String payload = "{\"log\":{\"timeStamp\":\"" + timeStamp + "\",\"function\":\"" + function + "\",\"message\":\"" + message + "\"}}";
            webSocket.sendTXT(i, payload);
        }
    }
}
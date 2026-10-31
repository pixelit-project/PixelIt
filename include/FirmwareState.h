#pragma once

#include "FirmwareFunctions.h"

class SetGPIO
{
public:
    int gpio;
    ulong resetMillis;
};
#define SET_GPIO_SIZE 4
extern SetGPIO setGPIOReset[SET_GPIO_SIZE];
extern bool mqttAktiv;
extern String mqttUser;
extern String mqttPassword;
extern String mqttServer;
extern String mqttMasterTopic;
extern String mqttDeviceTopic;
extern bool mqttUseDeviceTopic;
extern bool mqttHAdiscoverable;
extern int mqttPort;
extern unsigned long mqttLastReconnectAttempt;
extern unsigned long mqttLastService;
extern const int MQTT_RECONNECT_INTERVAL;
extern String dfpRXPin;
extern String dfpTXPin;
extern String onewirePin;
extern String SCLPin;
extern String SDAPin;
extern String ldrDevice;
extern unsigned long ldrPulldown;
extern unsigned int ldrSmoothing;
extern float batteryLevel;
extern unsigned long batteryLevelPrevMillis;
#ifndef MIN_BATTERY
#define MIN_BATTERY 0
#endif
#ifndef MAX_BATTERY
#define MAX_BATTERY 100
#endif
#define TELEMETRY_SERVER_HOST "pixelit.bastelbunker.de"
#define TELEMETRY_SERVER_PATH "/api/telemetry"
#define TELEMETRY_SERVER_PORT 80
#define CHECKUPDATE_SERVER_HOST "pixelit.bastelbunker.de"
#define CHECKUPDATE_SERVER_PATH "/api/lastversion"
#define CHECKUPDATE_SERVER_PORT 80
extern int btnPressedLevel[3];
extern const String btnAPINames[3];
extern const String btnLogNames[3];
extern btnStates btnState[3];
extern bool btnLastPublishState[3];
#if defined(ULANZI)
extern String btnPin[3];
extern bool btnEnabled[3];
extern btnActions btnAction[3];
#else
extern String btnPin[3];
extern bool btnEnabled[3];
extern btnActions btnAction[3];
#endif
extern CRGB leds[MATRIX_WIDTH * MATRIX_HEIGHT];
#if defined(ESP8266)
extern bool isESP8266;
#else
extern bool isESP8266;
#endif
#if defined(ESP32)
extern TwoWire twowire;
#elif defined(ULANZI)
extern TwoWire twowire;
#else
extern TwoWire twowire;
#endif
extern Adafruit_BME280 *bme280;
extern Adafruit_BMP280 *bmp280;
extern Adafruit_BME680 *bme680;
extern Adafruit_SHT31 sht31;
extern unsigned long lastBME680read;
extern DHTesp dht;
extern TempSensor tempSensor;
extern TemperatureUnit temperatureUnit;
extern LightDependentResistor *photocell;
extern BH1750 *bh1750;
extern Max44009 *max44009;
extern LuxSensor luxSensor;
extern FastLED_NeoMatrix *matrix;
extern WiFiClient wifiClientMQTT;
extern WiFiClient wifiClientHTTP;
extern WiFiUDP udp;
extern PubSubClient client;
extern WiFiManager wifiManager;
extern Ticker connectingAnimTicker;
extern int16_t connectingBarX;
extern int8_t connectingBarDir;
extern const uint8_t CONNECTING_TRAIL_LEN;
extern const uint8_t CONNECTING_TRAIL_BRIGHTNESS[6];
#if defined(ESP8266)
extern ESP8266WebServer server;
extern ESP8266HTTPUpdateServer httpUpdater;
#elif defined(ESP32)
extern WebServer server;
extern HTTPUpdateServer httpUpdater;
#endif
extern Liveview liveview;
extern String currentScreenJsonBuffer;
extern PixelItWebSockets webSocket;
extern DFPlayerMini_Fast mp3Player;
extern SoftwareSerial *softSerial;
extern uint initialVolume;
extern int currentMatrixBrightness;
extern bool matrixBrightnessAutomatic;
extern int mbaDimMin;
extern int mbaDimMax;
extern int mbaLuxMin;
extern int mbaLuxMax;
extern int matrixType;
extern String note;
extern String hostname;
extern String deviceID;
extern String matrixTempCorrection;
extern bool sleepMode;
extern bool bootScreenAktiv;
extern bool bootBatteryScreen;
extern bool bootSound;
extern String optionsVersion;
extern unsigned long lastScreenMessageMillis;
extern unsigned long lastGetBatteryPercent;
extern std::unique_ptr<uint16_t[]> bmpArray;
extern bool withBMP;
extern int bmpWidth;
extern int bmpHeight;
extern int bmpPosX;
extern int bmpPosY;
extern String ntpServer;
extern uint ntpRetryCounter;
extern unsigned long ntpTimeOut;
#define NTP_MAX_RETRYS 3
#define NTP_TIMEOUT_SEC 60
extern bool clockBlink;
extern bool clockAktiv;
extern bool clock24Hours;
extern bool clockDateDayMonth;
extern bool clockDayOfWeekFirstMonday;
extern bool clockDayLightSaving;
extern bool clockSwitchAktiv;
extern bool clockWithSeconds;
extern bool clockAutoFallbackActive;
extern uint clockAutoFallbackAnimation;
extern uint clockSwitchSec;
extern uint clockCounterClock;
extern uint clockCounterDate;
extern float clockTimeZone;
extern time_t clockLastUpdate;
extern uint8_t clockColorR, clockColorG, clockColorB;
extern uint clockAutoFallbackTime;
extern bool forceClock;
extern bool clockBlinkAnimated;
extern bool clockLargeFont;
extern bool clockFatFont;
extern bool clockDrawWeekDays;
extern bool scrollTextAktivLoop;
extern unsigned long scrollTextPrevMillis;
extern uint scrollTextDefaultDelay;
extern uint scrollTextDelay;
extern int scrollCurPos;
extern int scrollposY;
extern int scrollposX;
extern int scrollxTextWidth;
extern int scrollxAvailableTextSpace;
extern String scrollTextString;
extern std::unique_ptr<uint16_t[]> animationBmpList;
extern size_t animationBmpPixels;
extern size_t animationBmpCount;
extern bool animateBMPAktivLoop;
extern unsigned long animateBMPPrevMillis;
extern unsigned long animationLastRenderedMillis;
extern unsigned long animationMaxFrameGap;
extern uint32_t animationRenderedFrames;
extern uint32_t animationFailedLedFrames;
extern const char *animationSlowestTask;
extern unsigned long animationSlowestTaskMillis;
extern int animateBMPCounter;
extern bool animateBMPReverse;
extern bool animateBMPRubberbandingAktiv;
extern uint animateBMPDelay;
extern int animateBMPLimitLoops;
extern int animateBMPLoopCount;
extern int animateBMPLimitFrames;
extern int animateBMPFrameCount;
extern unsigned long sendLuxPrevMillis;
extern unsigned long getLuxPrevMillis;
extern unsigned long sendSensorPrevMillis;
extern unsigned long sendInfoPrevMillis;
extern String oldGetLuxSensor;
extern String oldGetSensor;
extern float currentLux;
extern float luxOffset;
extern float temperatureOffset;
extern float humidityOffset;
extern float pressureOffset;
extern float gasOffset;
extern bool sendTelemetry;
extern unsigned long sendTelemetryPrevMillis;
extern unsigned long forcedScreenIsActiveUntil;
extern bool checkUpdateScreen;
extern unsigned long checkUpdateScreenPrevMillis;
extern unsigned long checkUpdatePrevMillis;
extern String lastReleaseVersion;
extern String OldGetMP3PlayerInfo;
extern String websocketConnection[10];

extern String haDisplayText;

extern String haTextMessage, haTextColor, haTextFont, haTextAlignment, haTextScroll;

extern bool firmwareUpdateChecked;

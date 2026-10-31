#include "FirmwareState.h"

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
unsigned long mqttLastService = 0;
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

const String btnAPINames[]{"leftButton", "middleButton", "rightButton"};
const String btnLogNames[]{"Left button", "Middle button", "Right button"};

btnStates btnState[] = {btnState_Released, btnState_Released, btnState_Released};
bool btnLastPublishState[] = {false, false, false};

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
TempSensor tempSensor = TempSensor_None;

// TemperatureUnit
TemperatureUnit temperatureUnit = TemperatureUnit_Celsius;

LightDependentResistor *photocell;
BH1750 *bh1750;
Max44009 *max44009;
LuxSensor luxSensor = LuxSensor_LDR;

FastLED_NeoMatrix *matrix;
WiFiClient wifiClientMQTT;
WiFiClient wifiClientHTTP;
WiFiUDP udp;
PubSubClient client(wifiClientMQTT);
WiFiManager wifiManager;
Ticker connectingAnimTicker;
int16_t connectingBarX = 0;
int8_t connectingBarDir = 1;
const uint8_t CONNECTING_TRAIL_LEN = 6;
const uint8_t CONNECTING_TRAIL_BRIGHTNESS[] = {255, 180, 110, 60, 25, 10};
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

PixelItWebSockets webSocket(81);
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
std::unique_ptr<uint16_t[]> bmpArray;
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
std::unique_ptr<uint16_t[]> animationBmpList;
size_t animationBmpPixels = 64;
size_t animationBmpCount = 0;
bool animateBMPAktivLoop = false;
unsigned long animateBMPPrevMillis = 0;
unsigned long animationLastRenderedMillis = 0;
unsigned long animationMaxFrameGap = 0;
uint32_t animationRenderedFrames = 0;
uint32_t animationFailedLedFrames = 0;
const char *animationSlowestTask = "none";
unsigned long animationSlowestTaskMillis = 0;
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

// MP3Player Vars
String OldGetMP3PlayerInfo;

// Websoket Vars
String websocketConnection[10];


String haDisplayText;

String haTextMessage;
String haTextColor = "#FFFFFF";
String haTextFont = "Small";
String haTextAlignment = "Centered";
String haTextScroll = "Auto";

bool firmwareUpdateChecked = false;

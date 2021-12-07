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

#include <Arduino.h>
// BME Sensor
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
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
#include <LightDependentResistor.h>
#include <DHTesp.h>
#include <DFPlayerMini_Fast.h>
#include <SoftwareSerial.h>
#include "ColorConverterLib.h"
#include <TimeLib.h>
#include <ArduinoJson.h>
// PixelIT Stuff
#include "PixelItFont.h"
#include "Webinterface.h"
#include "Tools.h"

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
String mqttMasterTopic = "Haus/PixelIt/";
int mqttPort = 1883;
unsigned long mqttLastReconnectAttempt = 0; // will store last time reconnect to mqtt broker
const int MQTT_RECONNECT_INTERVAL = 5000;
//#define MQTT_MAX_PACKET_SIZE 8000

//// LDR Config
#define LDR_RESISTOR 10000 // ohms
#define LDR_PIN A0
#define LDR_PHOTOCELL LightDependentResistor::GL5516

//// GPIO Config
#if defined(ESP8266)
#define MATRIX_PIN D2
#define I2C_SDA D3
#define I2C_SCL D1
#define DHT_PIN D1
#elif defined(ESP32)
#define MATRIX_PIN 27
#define I2C_SDA D3
#define I2C_SCL D1
#define DHT_PIN D1
#endif

#define NUMMATRIX (32 * 8)
CRGB leds[NUMMATRIX];

#define VERSION "0.3.15"

#if defined(ESP32)
TwoWire twowire(BME280_ADDRESS_ALTERNATE);
#else
TwoWire twowire;
#endif
Adafruit_BME280 bme280;
Adafruit_BME680 *bme680;
unsigned long lastBME680read = 0;

FastLED_NeoMatrix *matrix;
WiFiClient espClient;
WiFiUDP udp;
PubSubClient client(espClient);
WiFiManager wifiManager;
#if defined(ESP8266)
ESP8266WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;
#elif defined(ESP32)
WebServer server(80);
HTTPUpdateServer httpUpdater;
#endif

WebSocketsServer webSocket = WebSocketsServer(81);
LightDependentResistor photocell(LDR_PIN, LDR_RESISTOR, LDR_PHOTOCELL, 10);
DHTesp dht;
DFPlayerMini_Fast mp3Player;
SoftwareSerial softSerial(D7, D8); // RX | TX

// TempSensor
enum TempSensor
{
	TempSensor_None,
	TempSensor_BME280,
	TempSensor_DHT,
	TempSensor_BME680,
};
TempSensor tempSensor = TempSensor_None;

// TemperatureUnit
enum TemperatureUnit
{
	TemperatureUnit_Celsius,
	TemperatureUnit_Fahrenheit
};
TemperatureUnit temperatureUnit = TemperatureUnit_Celsius;

// Matrix Vars
int currentMatrixBrightness = 127;
bool matrixBrightnessAutomatic = true;
int mbaDimMin = 20;
int mbaDimMax = 100;
int mbaLuxMin = 0;
int mbaLuxMax = 400;
int matrixType = 1;
String note;
String hostname;
String matrixTempCorrection = "default";

// System Vars
bool sleepMode = false;
bool bootScreenAktiv = true;
bool shouldSaveConfig = false;
String optionsVersion = "";
// Millis timestamp of the last receiving screen
unsigned long lastScreenMessageMillis = 0;

// Bmp Vars
uint16_t bmpArray[64];

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

// Scrolltext Vars
bool scrollTextAktivLoop = false;
unsigned long scrollTextPrevMillis = 0;
uint scrollTextDefaultDelay = 100;
uint scrollTextDelay;
int scrollPos;
int scrollposY;
bool scrollwithBMP;
int scrollxPixelText;
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
unsigned long sendSensorPrevMillis = 0;
unsigned long sendInfoPrevMillis = 0;
String oldGetMatrixInfo;
String oldGetLuxSensor;
String oldGetSensor;
float currentLux = 0.0f;
float luxOffset = 0.0f;
float temperatureOffset = 0.0f;
float humidityOffset = 0.0f;
float pressureOffset = 0.0f;
float gasOffset = 0.0f;

// MP3Player Vars
String OldGetMP3PlayerInfo;

// Websoket Vars
String websocketConnection[10];

void SaveConfigCallback()
{
	shouldSaveConfig = true;
}

void SetCurrentMatrixBrightness(float newBrightness)
{
	currentMatrixBrightness = newBrightness;
	matrix->setBrightness(currentMatrixBrightness);
}

void EnteredHotspotCallback(WiFiManager *manager)
{
	DrawTextHelper("HOTSPOT", false, false, false, false, false, false, NULL, 255, 255, 255, 3, 1);
}

void SaveConfig()
{
	// save the custom parameters to FS
	if (shouldSaveConfig)
	{
		DynamicJsonBuffer jsonBuffer;
		JsonObject &json = jsonBuffer.createObject();

		json["version"] = VERSION;
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
		json["scrollTextDefaultDelay"] = scrollTextDefaultDelay;
		json["bootScreenAktiv"] = bootScreenAktiv;
		json["mqttAktiv"] = mqttAktiv;
		json["mqttUser"] = mqttUser;
		json["mqttPassword"] = mqttPassword;
		json["mqttServer"] = mqttServer;
		json["mqttMasterTopic"] = mqttMasterTopic;
		json["mqttPort"] = mqttPort;
		json["luxOffset"] = luxOffset;
		json["temperatureOffset"] = temperatureOffset;
		json["humidityOffset"] = humidityOffset;
		json["pressureOffset"] = pressureOffset;
		json["gasOffset"] = gasOffset;

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
				SetConfigVaribles(json);
				Log("LoadConfig", "Loaded");
			}
		}
	}
	else
	{
		Log("LoadConfig", "No Configfile, init new file");
		SaveConfigCallback();
		SaveConfig();
	}
}

void SetConfig(JsonObject &json)
{
	SetConfigVaribles(json);
	SaveConfigCallback();
	SaveConfig();
}

void SetConfigVaribles(JsonObject &json)
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
		SetCurrentMatrixBrightness(json["matrixBrightness"].as<float>());
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
		hostname = json["hostname"].as<char *>();
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

	if (json.containsKey("scrollTextDefaultDelay"))
	{
		scrollTextDefaultDelay = json["scrollTextDefaultDelay"].as<uint>();
	}

	if (json.containsKey("bootScreenAktiv"))
	{
		bootScreenAktiv = json["bootScreenAktiv"].as<bool>();
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
	}

	if (json.containsKey("mqttPort"))
	{
		mqttPort = json["mqttPort"].as<int>();
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
}

void WifiSetup()
{
	wifiManager.resetSettings();
	ESP.restart();
	delay(300);
}

void HandleGetMainPage()
{
	server.sendHeader("Connection", "close");
	server.send(200, "text/html", mainPage);
}

void HandleNotFound()
{
	if (server.method() == HTTP_OPTIONS)
	{
		server.sendHeader("Access-Control-Allow-Origin", "*");
		server.send(204);
	}

	server.sendHeader("Location", "/update", true);
	server.send(302, "text/plain", "");
}

void HandleScreen()
{
	DynamicJsonBuffer jsonBuffer;
	JsonObject &json = jsonBuffer.parseObject(server.arg("plain"));
	server.sendHeader("Connection", "close");
	server.sendHeader("Access-Control-Allow-Origin", "*");

	if (json.success())
	{
		server.send(200, "application/json", "{\"response\":\"OK\"}");
		Log("HandleScreen", "Incomming Json length: " + String(json.measureLength()));
		CreateFrames(json);
	}
	else
	{
		server.send(406, "application/json", "{\"response\":\"Not Acceptable\"}");
	}
}

void Handle_wifisetup()
{
	WifiSetup();
}

void HandleSetConfig()
{
	DynamicJsonBuffer jsonBuffer;
	JsonObject &json = jsonBuffer.parseObject(server.arg("plain"));
	server.sendHeader("Connection", "close");

	if (json.success())
	{
		Log("SetConfig", "Incomming Json length: " + String(json.measureLength()));
		SetConfig(json);
		server.send(200, "application/json", "{\"response\":\"OK\"}");
		delay(500);
		ESP.restart();
	}
	else
	{
		server.send(406, "application/json", "{\"response\":\"Not Acceptable\"}");
	}
}

void HandleGetConfig()
{
	server.sendHeader("Connection", "close");
	server.send(200, "application/json", GetConfig());
}

void HandleGetLuxSensor()
{
	server.sendHeader("Connection", "close");
	server.send(200, "application/json", GetLuxSensor());
}

void HandelGetBrightness()
{
	server.sendHeader("Connection", "close");
	server.send(200, "application/json", GetBrightness());
}

void HandleGetDHTSensor() // Legancy
{
	server.sendHeader("Connection", "close");
	server.send(200, "application/json", GetSensor());
}

void HandleGetSensor()
{
	server.sendHeader("Connection", "close");
	server.send(200, "application/json", GetSensor());
}

void HandleGetMatrixInfo()
{
	server.sendHeader("Connection", "close");
	server.send(200, "application/json", GetMatrixInfo());
}

void Handle_factoryreset()
{
#if defined(ESP8266)
	File configFile = LittleFS.open("/config.json", "w");
#elif defined(ESP32)
	File configFile = SPIFFS.open("/config.json", "w");
#endif
	if (!configFile)
	{
		Log("Handle_factoryreset", "Failed to open config file for reset");
	}
	configFile.println("");
	configFile.close();
	WifiSetup();
	ESP.restart();
}

void callback(char *topic, byte *payload, unsigned int length)
{
	if (payload[0] == '{')
	{
		payload[length] = '\0';
		String channel = String(topic);
		channel.replace(mqttMasterTopic, "");

		DynamicJsonBuffer jsonBuffer;
		JsonObject &json = jsonBuffer.parseObject(payload);

		Log("MQTT_callback", "Incomming Json length: " + String(json.measureLength()));

		if (channel.equals("setScreen"))
		{
			CreateFrames(json);
		}
		else if (channel.equals("getLuxsensor"))
		{
			client.publish((mqttMasterTopic + "luxsensor").c_str(), GetLuxSensor().c_str());
		}
		else if (channel.equals("getMatrixinfo"))
		{
			client.publish((mqttMasterTopic + "matrixinfo").c_str(), GetMatrixInfo().c_str());
		}
		else if (channel.equals("getConfig"))
		{
			client.publish((mqttMasterTopic + "config").c_str(), GetConfig().c_str());
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
		// Merken für was die Connection hergstellt wurde
		websocketConnection[num] = String((char *)payload);

		// IP der Connection abfragen
		IPAddress ip = webSocket.remoteIP(num);

		// Logausgabe
		Log("WebSocketEvent", "[" + String(num) + "] Connected from " + ip.toString() + " url: " + websocketConnection[num]);

		// send message to client
		SendMatrixInfo(true);
		SendLDR(true);
		SendSensor(true);
		SendConfig();
		break;
	}
	case WStype_TEXT:
	{
		if (((char *)payload)[0] == '{')
		{
			DynamicJsonBuffer jsonBuffer;
			JsonObject &json = jsonBuffer.parseObject(payload);

			// Logausgabe
			Log("WebSocketEvent", "Incomming Json length: " + String(json.measureLength()));

			if (json.containsKey("setScreen"))
			{
				CreateFrames(json["setScreen"]);
			}
			else if (json.containsKey("setConfig"))
			{
				SetConfig(json["setConfig"]);
				delay(500);
				ESP.restart();
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
	String logMessage = F("Json contains ");

	// Ist eine Display Helligkeit übergeben worden?
	if (json.containsKey("brightness"))
	{
		logMessage += F("Brightness Control, ");
		currentMatrixBrightness = json["brightness"];
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

	// SleepMode
	if (json.containsKey("sleepMode"))
	{
		logMessage += F("SleepMode Control, ");
		sleepMode = json["sleepMode"];
	}
	// SleepMode
	if (sleepMode)
	{
		matrix->setBrightness(0);
		matrix->show();
	}
	else
	{
		matrix->setBrightness(currentMatrixBrightness);

		// Prüfung für die Unterbrechnung der lokalen Schleifen
		if (json.containsKey("bitmap") || json.containsKey("text") || json.containsKey("bar") || json.containsKey("bars") || json.containsKey("bitmapAnimation"))
		{
			lastScreenMessageMillis = millis();
			clockAktiv = false;
			scrollTextAktivLoop = false;
			animateBMPAktivLoop = false;
		}

		// Ist eine Switch Animation übergeben worden?
		bool fadeAnimationAktiv = false;
		bool coloredBarWipeAnimationAktiv = false;
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

		// Clock
		if (json.containsKey("clock"))
		{
			logMessage += F("InternalClock Control, ");
			if (json["clock"]["show"])
			{
				scrollTextAktivLoop = false;
				animateBMPAktivLoop = false;
				clockAktiv = true;

				clockCounterClock = 0;
				clockCounterDate = 0;

				clockSwitchAktiv = json["clock"]["switchAktiv"];

				if (clockSwitchAktiv == true)
				{
					clockSwitchSec = json["clock"]["switchSec"];
				}

				clockWithSeconds = json["clock"]["withSeconds"];

				if (json["clock"]["color"]["r"].as<char *>() != NULL)
				{
					clockColorR = json["clock"]["color"]["r"].as<uint8_t>();
					clockColorG = json["clock"]["color"]["g"].as<uint8_t>();
					clockColorB = json["clock"]["color"]["b"].as<uint8_t>();
				}
				else if (json["clock"]["hexColor"].as<char *>() != NULL)
				{
					ColorConverter::HexToRgb(json["clock"]["hexColor"].as<char *>(), clockColorR, clockColorG, clockColorB);
				}
				DrawClock(true);
			}
		}

		if (json.containsKey("bitmap") || json.containsKey("bitmapAnimation") || json.containsKey("text") || json.containsKey("bar") || json.containsKey("bars"))
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
				ColorConverter::HexToRgb(json["clock"]["hexColor"].as<char *>(), r, g, b);
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
				if (json["bar"]["hexColor"].as<char *>() != NULL)
				{
					ColorConverter::HexToRgb(json["clock"]["hexColor"].as<char *>(), r, g, b);
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

		// Ist ein Bitmap übergeben worden?
		if (json.containsKey("bitmap"))
		{
			logMessage += F("Bitmap, ");

			int16_t h = json["bitmap"]["size"]["height"].as<int16_t>();
			int16_t w = json["bitmap"]["size"]["width"].as<int16_t>();
			int16_t x = json["bitmap"]["position"]["x"].as<int16_t>();
			int16_t y = json["bitmap"]["position"]["y"].as<int16_t>();

			// Hier kann leider nicht die Funktion matrix->drawRGBBitmap() genutzt werde da diese Fehler in der Anzeige macht wenn es mehr wie 8x8 Pixel werden.
			for (int16_t j = 0; j < h; j++, y++)
			{
				for (int16_t i = 0; i < w; i++)
				{
					matrix->drawPixel(x + i, y, json["bitmap"]["data"][j * w + i].as<uint16_t>());
				}
			}

			// JsonArray in IntArray konvertieren
			// dies ist nötik für diverse kleine Logiken z.B. Scrolltext
			json["bitmap"]["data"].as<JsonArray>().copyTo(bmpArray);
		}

		// Ist eine BitmapAnimation übergeben worden?
		if (json.containsKey("bitmapAnimation"))
		{
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
			logMessage += F("Text");
			// Immer erstmal den Default Delay annehmen.
			scrollTextDelay = scrollTextDefaultDelay;

			// Ist ScrollText auto oder true gewählt?
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

			if (json["text"]["centerText"])
			{
				bool withBMP = json.containsKey("bitmap") || json.containsKey("bitmapAnimation");

				DrawTextCenter(json["text"]["textString"], json["text"]["bigFont"], withBMP, r, g, b);
			}
			// Ist ScrollText auto oder true gewählt?
			else if (scrollTextAktiv)
			{

				bool withBMP = (json.containsKey("bitmap") || json.containsKey("bitmapAnimation"));

				bool fadeInRequired = ((json.containsKey("bars") || json.containsKey("bar") || json.containsKey("bitmap") || json.containsKey("bitmapAnimation")) && fadeAnimationAktiv);

				// Wurde ein Benutzerdefeniertes Delay übergeben?
				if (json["text"]["scrollTextDelay"])
				{
					scrollTextDelay = json["text"]["scrollTextDelay"];
				}

				if (!(json["text"]["scrollText"]).is<bool>() && json["text"]["scrollText"] == "auto")
				{
					DrawAutoTextScrolled(json["text"]["textString"], json["text"]["bigFont"], withBMP, fadeInRequired, bmpArray, r, g, b);
				}
				else
				{
					DrawTextScrolled(json["text"]["textString"], json["text"]["bigFont"], withBMP, fadeInRequired, bmpArray, r, g, b);
				}
			}
			else
			{
				DrawText(json["text"]["textString"], json["text"]["bigFont"], r, g, b, json["text"]["position"]["x"], json["text"]["position"]["y"]);
			}
		}

		// Fade aktiv?
		if (!scrollTextAktiv && (fadeAnimationAktiv || coloredBarWipeAnimationAktiv))
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

	Log(F("CreateFrames"), logMessage);
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
		const float currentTemp = bme280.readTemperature();
		root["temperature"] = currentTemp + temperatureOffset;
		root["humidity"] = bme280.readHumidity() + humidityOffset;
		root["pressure"] = (bme280.readPressure() / 100.0F) + pressureOffset;
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

		const int elapsedSinceLastRead=millis()-lastBME680read;
		const int remain=bme680->remainingReadingMillis();

		if (remain==-1)  //no current values available
		{
			bme680->beginReading(); //start measurement process
			//return previous values
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

		if (remain>=0 ||elapsedSinceLastRead>20000)  
		//remain==0: measurement completed, not read yet
		//remain>0: measurement still running, but as we already are in the next loop call, block and read
		//elapsedSinceLastRead>20000: obviously, remain==-1. But as there haven't been loop calls recently, this seems to be an "infrequent" API call. Perform blocking read.
		{
			if (bme680->endReading())  //will become blocking if measurement not complete yet
			{
				lastBME680read=millis();
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
	else
	{
		root["humidity"] = "Not installed";
		root["temperature"] = "Not installed";
		root["pressure"] = "Not installed";
		root["gas"] = "Not installed";
	}

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
	root["freeSketchSpace"] = ESP.getFreeSketchSpace();
	root["wifiRSSI"] = WiFi.RSSI();
	root["wifiQuality"] = GetRSSIasQuality(WiFi.RSSI());
	root["wifiSSID"] = WiFi.SSID();
	root["ipAddress"] = WiFi.localIP().toString();
	root["freeHeap"] = ESP.getFreeHeap();

#if defined(ESP8266)
	root["sketchSize"] = ESP.getSketchSize();
	root["chipID"] = ESP.getChipId();
#elif defined(ESP32)
	root["chipID"] = uint64ToString(ESP.getEfuseMac());
#endif

	root["cpuFreqMHz"] = ESP.getCpuFreqMHz();
	root["sleepMode"] = sleepMode;

	String json;
	root.printTo(json);

	return json;
}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
void DrawText(String text, bool bigFont, int colorRed, int colorGreen, int colorBlue, int posX, int posY)
{
	DrawTextHelper(text, bigFont, false, false, false, false, false, NULL, colorRed, colorGreen, colorBlue, posX, posY);
}

void DrawTextCenter(String text, bool bigFont, bool withBMP, int colorRed, int colorGreen, int colorBlue)
{
	DrawTextHelper(text, bigFont, true, false, false, withBMP, false, NULL, colorRed, colorGreen, colorBlue, 0, 1);
}

void DrawTextScrolled(String text, bool bigFont, bool withBMP, bool fadeInRequired, uint16_t bmpArray[64], int colorRed, int colorGreen, int colorBlue)
{
	DrawTextHelper(text, bigFont, false, true, false, withBMP, fadeInRequired, bmpArray, colorRed, colorGreen, colorBlue, 0, 1);
}

void DrawAutoTextScrolled(String text, bool bigFont, bool withBMP, bool fadeInRequired, uint16_t bmpArray[64], int colorRed, int colorGreen, int colorBlue)
{
	DrawTextHelper(text, bigFont, false, false, true, withBMP, fadeInRequired, bmpArray, colorRed, colorGreen, colorBlue, 0, 1);
}

void DrawTextHelper(String text, bool bigFont, bool centerText, bool scrollText, bool autoScrollText, bool withBMP, bool fadeInRequired, uint16_t bmpArray[64], int colorRed, int colorGreen, int colorBlue, int posX, int posY)
{
	uint16_t xPixelText, xPixel;

	text = Utf8ToAscii(text);

	if (withBMP)
	{
		// Verfügbare Text Pixelanzahl in der Breite (X) mit Bild
		xPixel = 24;
		posX = 8;
	}
	else
	{
		// Verfügbare Text Pixelanzahl in der Breite (X) ohne Bild
		xPixel = 32;
	}

	if (bigFont)
	{
		// Grosse Schrift setzten
		matrix->setFont();
		xPixelText = text.length() * 6;
		// Positions Korrektur
		posY = posY - 1;
	}
	else
	{
		// Kleine Schrift setzten
		matrix->setFont(&PixelItFont);
		xPixelText = text.length() * 4;
		// Positions Korrektur
		posY = posY + 5;
	}

	// matrix->getTextBounds(text, 0, 0, &x1, &y1, &xPixelText, &h);

	if (centerText)
	{
		// Kein Offset benötigt.
		int offset = 0;

		// Mit BMP berechnen
		if (withBMP)
		{
			// Offset um nicht das BMP zu überschreiben
			// + 1 weil wir der erste pixel 0 ist!!!!
			offset = 8 + 1;
		}

		// Berechnung für das erste Pixel des Textes
		posX = ((xPixel - xPixelText) / 2) + offset;
	}

	matrix->setCursor(posX, posY);

	matrix->setTextColor(matrix->Color(colorRed, colorGreen, colorBlue));

	if (scrollText || (autoScrollText && xPixelText > xPixel))
	{

		matrix->setBrightness(currentMatrixBrightness);

		scrollTextString = text;
		scrollposY = posY;
		scrollwithBMP = withBMP;
		scrollxPixelText = xPixelText;
		// + 8 Pixel sonst scrollt er mitten drinn los!
		scrollPos = 33;

		scrollTextAktivLoop = true;
		scrollTextPrevMillis = millis();
		ScrollText(fadeInRequired);
	}
	// Fall doch der Text auf denm Display passt!
	else if (autoScrollText)
	{
		matrix->print(text);
		// Hier muss geprüfter weden ob ausgefadet wurde,
		// dann ist nämlich die Brightness zu weit unten (0),
		// aber auch nur wenn nicht animateBMPAktivLoop aktiv ist,
		// denn sonst hat er das schon erledigt.
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
	int tempxPixelText = 0;

	if (scrollxPixelText < 32 && !scrollwithBMP)
	{
		scrollxPixelText = 32;
	}
	else if (scrollxPixelText < 24 && scrollwithBMP)
	{
		scrollxPixelText = 24;
	}
	else if (scrollwithBMP)
	{
		tempxPixelText = 8;
	}

	if (scrollPos > ((scrollxPixelText - tempxPixelText) * -1))
	{
		matrix->clear();
		matrix->setCursor(--scrollPos, scrollposY);
		matrix->print(scrollTextString);

		if (scrollwithBMP)
		{
			matrix->drawRGBBitmap(0, 0, bmpArray, 8, 8);
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
		// + 8 Pixel sonst scrollt er mitten drinn los!
		scrollPos = 33;
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

	matrix->drawRGBBitmap(0, 0, animationBmpList[animateBMPCounter], 8, 8);

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

void DrawClock(bool fromJSON)
{
	matrix->clear();

	char date[14];
	char time[14];

	int xPosTime = 0;

	sprintf_P(date, PSTR("%02d.%02d."), day(), month());

	if (clock24Hours && clockWithSeconds)
	{
		xPosTime = 2;
		sprintf_P(time, PSTR("%02d:%02d:%02d"), hour(), minute(), second());
	}
	else if (!clock24Hours)
	{
		xPosTime = 2;

		if (clockBlink)
		{
			clockBlink = false;
			sprintf_P(time, PSTR("%02d:%02d %s"), hourFormat12(), minute(), isAM() ? "AM" : "PM");
		}
		else
		{
			clockBlink = !clockBlink;
			sprintf_P(time, PSTR("%02d %02d %s"), hourFormat12(), minute(), isAM() ? "AM" : "PM");
		}
	}
	else
	{
		xPosTime = 7;

		if (clockBlink)
		{
			clockBlink = false;
			sprintf_P(time, PSTR("%02d:%02d"), hour(), minute());
		}
		else
		{
			clockBlink = !clockBlink;
			sprintf_P(time, PSTR("%02d %02d"), hour(), minute());
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

			int counter = 0;
			while (counter <= 6)
			{
				counter++;
				matrix->clear();
				DrawText(String(time), false, clockColorR, clockColorG, clockColorB, xPosTime, (1 + counter));
				DrawText(String(date), false, clockColorR, clockColorG, clockColorB, 7, (-6 + counter));
				matrix->drawLine(0, 7, 33, 7, 0);
				DrawWeekDay();
				matrix->show();
				delay(35);
			}
		}
		else
		{
			DrawText(String(time), false, clockColorR, clockColorG, clockColorB, xPosTime, 1);
		}
	}
	else
	{
		clockCounterDate++;

		if (clockCounterDate == clockSwitchSec)
		{
			clockCounterClock = 0;

			int counter = 0;
			while (counter <= 6)
			{
				counter++;
				matrix->clear();
				DrawText(String(date), false, clockColorR, clockColorG, clockColorB, 7, (1 + counter));
				DrawText(String(time), false, clockColorR, clockColorG, clockColorB, xPosTime, (-6 + counter));
				matrix->drawLine(0, 7, 33, 7, 0);
				DrawWeekDay();
				matrix->show();
				delay(35);
			}
		}
		else
		{
			DrawText(String(date), false, clockColorR, clockColorG, clockColorB, 7, 1);
		}
	}

	DrawWeekDay();

	// Wenn der Aufruf nicht über JSON sondern über den Loop kommt
	// muss ich mich selbst ums Show kümmern.
	if (!fromJSON)
	{
		matrix->show();
	}
}

void DrawWeekDay()
{
	for (int i = 0; i <= 6; i++)
	{
		if (i == DayOfWeekFirstMonday(dayOfWeek(now()) - 1))
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
		Log(F("MQTTreconnect"), F("MQTT connecting to broker with user and password"));
		connected = client.connect(hostname.c_str(), mqttUser.c_str(), mqttPassword.c_str(), "state", 0, true, "diconnected");
	}
	else
	{
		Log(F("MQTTreconnect"), F("MQTT connecting to broker without user and password"));
		connected = client.connect(hostname.c_str(), "state", 0, true, "diconnected");
	}

	// Attempt to connect
	if (connected)
	{
		Log(F("MQTTreconnect"), F("MQTT connected!"));
		// ... and resubscribe
		client.subscribe((mqttMasterTopic + "setScreen").c_str());
		client.subscribe((mqttMasterTopic + "getLuxsensor").c_str());
		client.subscribe((mqttMasterTopic + "getMatrixinfo").c_str());
		client.subscribe((mqttMasterTopic + "getConfig").c_str());
		client.subscribe((mqttMasterTopic + "setConfig").c_str());
		// ... and publish
		client.publish((mqttMasterTopic + "state").c_str(), "connected");
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
	DrawTextHelper("PIXEL IT", false, false, false, false, false, false, NULL, 255, 255, 255, 3, 1);
	FadeIn(60, 10);
	FadeOut(60, 10);
	FadeIn(60, 10);
	FadeOut(60, 10);
	FadeIn(60, 10);
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
	int idx = 7 + OrigDayofWeek - 1;
	if (idx > 6) // week ends at 6, because Enum.DayOfWeek is zero-based
	{
		idx -= 7;
	}
	return idx;
}

/////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////
void setup()
{

	Serial.begin(115200);

	// Mounting FileSystem
	Serial.println(F("Mounting file system..."));
#if defined(ESP8266)
	if (LittleFS.begin())
#elif defined(ESP32)
	if (SPIFFS.begin())
#endif
	{
		Serial.println(F("Mounted file system."));
		LoadConfig();
		// If new version detected, create new variables in config if necessary.
		if (optionsVersion != VERSION)
		{
			Log(F("LoadConfig"), F("New version detected, create new variables in config if necessary"));
			SaveConfigCallback();
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

	// Temp Sensors
	twowire.begin(I2C_SDA, I2C_SCL);
	if (bme280.begin(BME280_ADDRESS_ALTERNATE, &twowire))
	{
		Log(F("Setup"), F("BME280 started"));
		tempSensor = TempSensor_BME280;
	}
	else
	{
		bme680 = new Adafruit_BME680(&twowire);
		if (bme680->begin())
		{
			Log(F("Setup"), F("BME680 started"));
			tempSensor = TempSensor_BME680;
		}
		else
		{
			delete bme680;
			// AM2320 needs a delay to be reliably initialized
			delay(600);
			dht.setup(DHT_PIN, DHTesp::DHT22);
			if (!isnan(dht.getHumidity()) && !isnan(dht.getTemperature()))
			{
				Log(F("Setup"), F("DHT started"));
				tempSensor = TempSensor_DHT;
			}
			else
			{
				Log(F("Setup"), F("No BME280, BME 680 or DHT Sensor found"));
			}
		}
	}

	// Matix Type 1 (Colum major)
	if (matrixType == 1)
	{
		matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG);
	}
	// Matix Type 2 (Row major)
	else if (matrixType == 2)
	{
		matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_ROWS + NEO_MATRIX_ZIGZAG);
	}
	// Matix Type 3 (Tiled 4x 8x8 CJMCU)
	else if (matrixType == 3)
	{
		matrix = new FastLED_NeoMatrix(leds, 32, 8, NEO_MATRIX_BOTTOM + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS + NEO_MATRIX_PROGRESSIVE);
	}

	// Init LightSensor
	photocell.setPhotocellPositionOnGround(false);
	ColorTemperature userColorTemp = GetUserColorTemp();
	LEDColorCorrection userLEDCorrection = GetUserColorCorrection();

	// Matrix Color Correction
	if (userLEDCorrection != UncorrectedColor)
	{
		FastLED.addLeds<NEOPIXEL, MATRIX_PIN>(leds, NUMMATRIX).setCorrection(userLEDCorrection);
	}
	else if (userColorTemp != UncorrectedTemperature)
	{
		FastLED.addLeds<NEOPIXEL, MATRIX_PIN>(leds, NUMMATRIX).setTemperature(userColorTemp);
	}
	else
	{
		int *rgbArray = GetUserCutomCorrection();
		FastLED.addLeds<NEOPIXEL, MATRIX_PIN>(leds, NUMMATRIX).setCorrection(matrix->Color(rgbArray[0], rgbArray[1], rgbArray[2]));
	}

	matrix->begin();
	matrix->setTextWrap(false);
	matrix->setBrightness(currentMatrixBrightness);
	matrix->clear();

	// Bootscreen
	if (bootScreenAktiv)
	{
		ShowBootAnimation();
	}

	// Hostname
	if (hostname.isEmpty())
	{
		hostname = "PixelIt";
	}
	WiFi.hostname(hostname);

	// Set config save notify callback
	wifiManager.setSaveConfigCallback(SaveConfigCallback);
	wifiManager.setAPCallback(EnteredHotspotCallback);
	wifiManager.setMinimumSignalQuality();
	// Config menue timeout 180 seconds.
	wifiManager.setConfigPortalTimeout(180);

	if (!wifiManager.autoConnect("PIXEL_IT"))
	{
		Log(F("Setup"), F("Wifi failed to connect and hit timeout"));
		delay(3000);
		// Reset and try again, or maybe put it to deep sleep
		ESP.restart();
		delay(5000);
	}

	Log(F("Setup"), F("Wifi connected...yeey :)"));
	SaveConfig();

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
	server.on(F("/api/brightness"), HTTP_GET, HandelGetBrightness);
	server.on(F("/api/dhtsensor"), HTTP_GET, HandleGetDHTSensor); // Legancy
	server.on(F("/api/sensor"), HTTP_GET, HandleGetSensor);
	server.on(F("/api/matrixinfo"), HTTP_GET, HandleGetMatrixInfo);
	// server.on(F("/api/soundinfo"), HTTP_GET, HandleGetSoundInfo);
	server.on(F("/api/config"), HTTP_POST, HandleSetConfig);
	server.on(F("/api/config"), HTTP_GET, HandleGetConfig);
	server.on(F("/"), HTTP_GET, HandleGetMainPage);
	server.onNotFound(HandleNotFound);

	server.begin();

	webSocket.begin();
	webSocket.onEvent(webSocketEvent);

	Log(F("Setup"), F("Webserver started"));

	if (mqttAktiv == true)
	{
		client.setServer(mqttServer.c_str(), mqttPort);
		client.setCallback(callback);
		client.setBufferSize(8000);
		Log(F("Setup"), F("MQTT started"));
	}

	softSerial.begin(9600);
	Log(F("Setup"), F("Software Serial started"));

	mp3Player.begin(softSerial);
	Log(F("Setup"), F("DFPlayer started"));
}

void loop()
{

	server.handleClient();
	webSocket.loop();

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

	// Clock Auto Fallback
	if (!sleepMode && clockAutoFallbackActive && !clockAktiv && millis() - lastScreenMessageMillis >= (clockAutoFallbackTime * 1000))
	{
		scrollTextAktivLoop = false;
		animateBMPAktivLoop = false;

		if (clockAutoFallbackAnimation == 1)
		{
			FadeOut();
		}
		else if (clockAutoFallbackAnimation == 2)
		{
			ColoredBarWipe();
		}

		clockAktiv = true;
		DrawClock(true);

		if (clockAutoFallbackAnimation != 0)
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

	if (millis() - sendLuxPrevMillis >= 1000)
	{
		sendLuxPrevMillis = millis();

		currentLux = (roundf(photocell.getCurrentLux() * 1000) / 1000) + luxOffset;

		SendLDR(false);

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
			}
		}
	}

	if (millis() - sendSensorPrevMillis >= 3000)
	{
		sendSensorPrevMillis = millis();
		SendSensor(false);
	}

	if (millis() - sendInfoPrevMillis >= 3000)
	{
		sendInfoPrevMillis = millis();
		SendMatrixInfo(false);
		// SendMp3PlayerInfo(false);
	}

	if (animateBMPAktivLoop && millis() - animateBMPPrevMillis >= animateBMPDelay)
	{
		animateBMPPrevMillis = millis();
		AnimateBMP(true);
	}

	if (scrollTextAktivLoop && millis() - scrollTextPrevMillis >= scrollTextDelay)
	{
		scrollTextPrevMillis = millis();
		ScrollText(false);
	}
}

void SendMatrixInfo(bool force)
{
	if (force)
	{
		oldGetMatrixInfo = "";
	}

	String matrixInfo;

	// Prüfen ob die ermittlung der MatrixInfo überhaupt erforderlich ist
	if ((mqttAktiv == true && client.connected()) || (webSocket.connectedClients() > 0))
	{
		matrixInfo = GetMatrixInfo();
	}
	// Prüfen ob über MQTT versendet werden muss
	if (mqttAktiv == true && client.connected() && oldGetMatrixInfo != matrixInfo)
	{
		client.publish((mqttMasterTopic + "matrixinfo").c_str(), matrixInfo.c_str(), true);
	}
	// Prüfen ob über Websocket versendet werden muss
	if (webSocket.connectedClients() > 0 && oldGetMatrixInfo != matrixInfo)
	{
		for (uint i = 0; i < sizeof websocketConnection / sizeof websocketConnection[0]; i++)
		{
			webSocket.sendTXT(i, "{\"sysinfo\":" + matrixInfo + "}");
		}
	}

	oldGetMatrixInfo = matrixInfo;
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

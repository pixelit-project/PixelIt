#pragma once

#include <Arduino.h>

String IntFormat(int inputInt);
boolean IsSummertime(int year, byte month, byte day, byte hour, byte clockTimeZone);
int DSToffset(time_t date, float clockTimeZone);
boolean isIP(String str);
byte Utf8ToAscii(byte ascii);
String Utf8ToAscii(String _str);
String uint64ToString(uint64_t input);
String GetChipID();
int GetRSSIasQuality(int rssi);
float CelsiusToFahrenheit(float celsius);
String RGBtoHEX(int r, int g, int b);
void HEXtoRGB(String hex, uint8_t &r, uint8_t &g, uint8_t &b);

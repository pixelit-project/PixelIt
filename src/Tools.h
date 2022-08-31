#include <Arduino.h>

/// <summary>
/// Adds a leading 0 to a number if it is smaller than 10
/// </summary>
String IntFormat(int inputInt)
{
	if (inputInt < 10)
	{
		return "0" + String(inputInt);
	}

	return String(inputInt);
}

boolean IsSummertime(int year, byte month, byte day, byte hour, byte clockTimeZone)
{
	if (month < 3 || month > 10)
	{
		return false; // keine Sommerzeit in Jan, Feb, Nov, Dez
	}
	if (month > 3 && month < 10)
	{
		return true; // Sommerzeit in Apr, Mai, Jun, Jul, Aug, Sep
	}
	// if (month == 3 && (hour + 24 * day) >= (1 + clockTimeZone + 24 * (31 - (5 * year / 4 + 4) % 7)) || month == 10 && (hour + 24 * day) < (1 + clockTimeZone + 24 * (31 - (5 * year / 4 + 1) % 7)))
	if ((month == 3 && (hour + 24 * day) >= (1 + clockTimeZone + 24 * (31 - (5 * year / 4 + 4) % 7))) || (month == 10 && (hour + 24 * day) < (1 + clockTimeZone + 24 * (31 - (5 * year / 4 + 1) % 7))))
	{
		return true;
	}
	else
	{
		return false;
	}
}

/// <summary>
/// Returns 1 hour in daylight saving time and outside 0
/// </summary>
int DSToffset(time_t date, float clockTimeZone)
{
	return IsSummertime(year(date), month(date), day(date), hour(date), clockTimeZone) ? 1 : 0;
}

/// <summary>
/// Checks if it is a valid IP address
/// </summary>
boolean isIP(String str)
{
	for (char i = 0; i < str.length(); i++)
	{
		if (!(isDigit(str.charAt(i)) || str.charAt(i) == '.'))
		{
			return false;
		}
	}
	return true;
}

/// <summary>
/// Convert UTF8 byte to ASCII
/// </summary>
byte Utf8ToAscii(byte ascii)
{
	static unsigned long fullSeq; // Full UTF-8 Byte Sequences
	static int firstBytePos;	  // Pos of the 1st Byte
	unsigned long tmpSeq;		  // Buffer to shift UTF-8 Byte Sequences while extracting
	int maxByteSeq = 0;			  // Holds legal UTF-8 Byte Sequences lenght
	byte result = 0;			  // Function result
	byte bytes[4] = {0, 0, 0, 0}; // Holds 1-4 Bytes from Full UTF-8 Byte Sequences

	// Shift UTF-8 Byte Sequences to right
	// Add new ASCII byte at the end
	fullSeq = fullSeq << 8;
	fullSeq = fullSeq | ascii;

	// Extract 1-4 Byte from UTF-8 Byte Sequences
	// and store them at the right postion
	// 0 = 1st Byte
	// 1 = 2nd Byte
	// 2 = 3rd Byte
	// 3 =  4th Byte
	tmpSeq = fullSeq;
	for (int i = firstBytePos; i >= 0; i--)
	{
		bytes[i] = tmpSeq & 0xff;
		tmpSeq = tmpSeq >> 8;
	}

	// Set where we find the 1st Byte
	firstBytePos += 1;

	switch (fullSeq)
	{
	case 0 ... 127: // Basic Latin (Decimal 0-127)
		result = ascii;
		break;
	case 0xC2A1 ... 0xC2BF: // Latin1 Supplement (Decimal 160-191)
		result = bytes[1] - 34;
		break;
	case 0xC380 ... 0xC3BF:
		result = (bytes[1] | 0xC0) - 34;
		break;
	case 0xE282AC: // Euro €
		result = 0xDE;
		break;
	case 0xE28690: // Arrow left ←
		result = 0xDF;
		break;
	case 0xE28691: // Arrow up ↑
		result = 0xE0;
		break;
	case 0xE28692: // Arrow right →
		result = 0xE1;
		break;
	case 0xE28693: // Arrow down ↓
		result = 0xE2;
		break;
	case 0XE29885: // Star ★
		result = 0xE3;
		break;
	case 0xF09F9381: // File 📁
		result = 0xE4;
		break;
	case 0xE299A5: // Heart ♥
		result = 0xE5;
		break;
	case 0xE286A7: // Download ↧
		result = 0xE6;
		break;
	case 0xF09F9A97: // Car 🚗
		result = 0xE7;
		break;
	case 0xF09F9880: // Smiley 😀
		result = 0xE8;
		break;
	}

	// Legal UTF-8 Byte Sequences
	// https://www.unicode.org/versions/corrigendum1.html
	// Check 1st Byte
	switch (bytes[0])
	{
	case 0x00 ... 0x7F:
		maxByteSeq = 1;
		break;
	case 0xC2 ... 0xDF:
		maxByteSeq = 2;
		break;
	case 0xE0 ... 0xEF:
		maxByteSeq = 3;
		break;
	case 0xF0 ... 0xF4:
		maxByteSeq = 4;
		break;
	default:
		maxByteSeq = 0;
		break;
	}

	// Reset static cache and shift if we have an
	// result or we exceed Legal UTF-8 Byte Sequences
	if (result != 0 || firstBytePos >= maxByteSeq)
	{
		firstBytePos = 0;
		fullSeq = 0;
	}

	return result;
}

/// <summary>
/// Convert UTF8 Chars to ASCII
/// </summary>
String Utf8ToAscii(String _str)
{
	String result = "";
	char thisChar;

	for (unsigned int i = 0; i < _str.length(); i++)
	{
		thisChar = Utf8ToAscii(_str.charAt(i));

		if (thisChar != 0)
		{
			result += thisChar;
		}
	}
	return result;
}

String uint64ToString(uint64_t input)
{
	String result = "";
	uint8_t base = 10;

	do
	{
		char c = input % base;
		input /= base;

		if (c < 10)
			c += '0';
		else
			c += 'A' - 10;
		result = c + result;
	} while (input);
	return result;
}

/// <summary>
/// Returns the chip id
/// </summary>
String GetChipID()
{
#if defined(ESP8266)
	return String(ESP.getChipId());
#elif defined(ESP32)
	return uint64ToString(ESP.getEfuseMac());
#endif
}

/// <summary>
/// Convert RSSI to percentage quality
/// </summary>
int GetRSSIasQuality(int rssi)
{
	int quality = 0;

	if (rssi <= -100)
	{
		quality = 0;
	}
	else if (rssi >= -50)
	{
		quality = 100;
	}
	else
	{
		quality = 2 * (rssi + 100);
	}
	return quality;
}

/// <summary>
/// Convert celsius to fahrenheit
/// </summary>
float CelsiusToFahrenheit(float celsius)
{
	return (celsius * 9 / 5) + 32;
}
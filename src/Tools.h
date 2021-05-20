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

/// <summary>
/// Returns 1 hour in daylight saving time and outside 0
/// </summary>
int DSToffset(time_t date, float clockTimeZone)
{
	bool _summerTime;

	if (month(date) < 3 || month(date) > 10)
	{
		_summerTime = false; // no summertime in Jan, Feb, Nov, Dec
	}
	else if (month(date) > 3 && month(date) < 10)
	{
		_summerTime = true; // Summertime in Apr, May, Jun, Jul, Aug, Sep
	}
	else if ((month(date) == 3 && (hour(date) + 24 * day(date)) >= (1 + clockTimeZone + 24 * (31 - (5 * year(date) / 4 + 4) % 7))) || (month(date) == 10 && (hour(date) + 24 * day(date))) < (1 + clockTimeZone + 24 * (31 - (5 * year(date) / 4 + 1) % 7)))
	{
		_summerTime = true;
	}
	else
	{
		_summerTime = false;
	}
	return _summerTime ? 1 : 0;
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
	static byte thisByte;

	if (ascii < 128)
	{
		thisByte = 0;
		return (ascii);
	}

	byte last = thisByte;
	thisByte = ascii;
	byte result = 0;

	switch (last)
	{
	case 0xC2:
		result = ascii - 34;
		break;
	case 0xC3:
		result = (ascii | 0xC0) - 34;
		break;
	case 0x82:
		if (ascii == 0xAC)
		{
			result = (0xEA);
		}
		break;
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

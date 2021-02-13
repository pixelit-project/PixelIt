/// <summary>
/// Adds a leading 0 to a number if it is smaller than 10
/// </summary>
String IntFormat(int _int)
{
	if (_int < 10)
	{
		return "0" + String(_int);
	}

	return String(_int);
}

/// <summary>
/// Returns 1 hour in daylight saving time and outside 0
/// </summary>
int DSToffset(time_t _date, int _clockTimeZone)
{
	bool _summerTime;

	if (month(_date) < 3 || month(_date) > 10)
	{
		_summerTime = false; // no summertime in Jan, Feb, Nov, Dec
	}
	else if (month(_date) > 3 && month(_date) < 10) {
		_summerTime = true; // Summertime in Apr, May, Jun, Jul, Aug, Sep
	}
	else if (month(_date) == 3 && (hour(_date) + 24 * day(_date)) >= (1 + String(_clockTimeZone).toInt() + 24 * (31 - (5 * year(_date) / 4 + 4) % 7)) || month(_date) == 10 && (hour(_date) + 24 * day(_date)) < (1 + String(_clockTimeZone).toInt() + 24 * (31 - (5 * year(_date) / 4 + 1) % 7)))
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
boolean isIP(String _str) {
	for (char i = 0; i < _str.length(); i++) {
		if (!(isDigit(_str.charAt(i)) || _str.charAt(i) == '.')) {
			return false;
		}
	}
	return true;
}

/// <summary>
/// Convert UTF8 byte to ASCII
/// </summary>
byte Utf8ToAscii(byte _ascii) {
	static byte _thisByte;

	if (_ascii < 128)
	{
		_thisByte = 0;
		return (_ascii);
	}

	byte last = _thisByte;
	_thisByte = _ascii;
	byte _result = 0;

	switch (last)
	{
	case 0xC2:
		_result = _ascii - 34;
		break;
	case 0xC3:
		_result = (_ascii | 0xC0) - 34;
		break;
	case 0x82:
		if (_ascii == 0xAC)
		{
			_result = (0xEA);
		}
		break;
	}
	return  _result;
}

/// <summary>
/// Convert UTF8 Chars to ASCII
/// </summary>
String Utf8ToAscii(String _str) {
	String _result = "";
	char _thisChar;

	for (int i = 0; i < _str.length(); i++)
	{
		_thisChar = Utf8ToAscii(_str.charAt(i));

		if (_thisChar != 0)
		{
			_result += _thisChar;
		}
	}
	return _result;
}

String uint64ToString(uint64_t input) {
  String result = "";
  uint8_t base = 10;

  do {
    char c = input % base;
    input /= base;

    if (c < 10)
      c +='0';
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

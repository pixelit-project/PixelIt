String IntFormat(int _int)
{
	if (_int < 10)
	{
		return "0" + String(_int);
	}

	return String(_int);
}

int DSToffset(time_t _date, int _clockTimeZone)
{
	bool _summerTime;

	if (month(_date) < 3 || month(_date) > 10)
	{
		_summerTime = false; // keine Sommerzeit in Jan, Feb, Nov, Dez
	}
	else if (month(_date) > 3 && month(_date) < 10) {
		_summerTime = true; // Sommerzeit in Apr, Mai, Jun, Jul, Aug, Sep
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

boolean isIP(String _str) {
	for (char i = 0; i < _str.length(); i++) {
		if (!(isDigit(_str.charAt(i)) || _str.charAt(i) == '.')) {
			return false;
		}
	}
	return true;
}

byte Utf8ToAscii(byte _ascii) {
	static byte thisByte;

	if (_ascii < 128)
	{
		thisByte = 0;
		return (_ascii);
	}

	byte last = thisByte;
	thisByte = _ascii;
	byte result = 0;

	switch (last)
	{
	case 0xC2:
		result = _ascii - 34;
		break;
	case 0xC3:
		result = (_ascii | 0xC0) - 34;
		break;
	case 0x82:
		if (_ascii == 0xAC)
		{
			result = (0xEA);
		}
		break;
	}
	return  result;
}

String Utf8ToAscii(String _str) {
	String result = "";
	char thisChar;

	for (int i = 0; i < _str.length(); i++)
	{
		thisChar = Utf8ToAscii(_str.charAt(i));

		if (thisChar != 0)
		{
			result += thisChar;
		}
	}
	return result;
}


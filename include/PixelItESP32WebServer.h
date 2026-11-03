#pragma once
#include <WebServer.h>

// The pinned ESP32 WebServer returns arguments by value. Expose the owned
// screen body for in-place JSON parsing without another request-sized copy.
class PixelItESP32WebServer : public WebServer
{
public:
    explicit PixelItESP32WebServer(int port) : WebServer(port) {}
    String &screenBody()
    {
        for (int i = 0; i < _currentArgCount; ++i)
            if (_currentArgs[i].key == "plain") return _currentArgs[i].value;
        return _missingBody;
    }
private:
    String _missingBody;
};

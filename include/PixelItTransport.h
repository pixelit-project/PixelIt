#pragma once
#include <stddef.h>
#include <stdint.h>
#include "ScreenJsonCompactor.h"
#include "ScreenLimits.h"

void ServiceTransportReceive();
#if defined(ESP32)
size_t QueueSocketWrite(uint8_t client, int fd, const uint8_t *data, size_t length);
void ResetSocketOutput(uint8_t client, bool enabled = false);
bool SocketOutputEnabled(uint8_t client);
void FlushSocketOutput();
bool SocketPreviewReady(uint8_t client);
#endif
void RecordSocketWriteError(uint8_t client, unsigned status, int error);
void RecordTransportReceive(bool mqtt, unsigned status, size_t input, size_t output);
void NotifySocketReceiveError(uint8_t client, unsigned status);
bool MqttReceiveAccepted();

// No allocation: compaction writes directly into the caller's bounded buffer.
class CompactTransportBuffer
{
public:
    CompactTransportBuffer(uint8_t *data, size_t capacity, bool json = true) : data(data), capacity(capacity), mode(json ? 0 : 1), compact(*this) {}
    bool put(char c)
    {
        if (!data || size >= capacity) { status = 503; return false; }
        data[size++] = c;
        return true;
    }
    void feed(char c)
    {
        if (status != 200) return;
        if (!mode && !ScreenJsonSpace(c)) mode = c == '{' ? 2 : 1;
        if (mode == 2)
        {
            if (!compact.put(c) && status == 200) status = 406;
        }
        else put(c); // Plain HA text and binary/non-JSON payloads stay byte-exact.
    }
    unsigned finish() { if (status == 200 && mode == 2 && !compact.complete()) status = 406; return status; }
    bool jsonCommand() const { return mode == 2; }
    size_t size = 0;
    unsigned status = 200;
private:
    uint8_t *data;
    size_t capacity;
    unsigned mode = 0;
    ScreenJsonCompactor<CompactTransportBuffer> compact;
};

#if defined(ESP8266) || defined(ESP32)
class WiFiClient;
unsigned ReadCompactSocket(WiFiClient &client, uint8_t *output, size_t capacity,
                           size_t length, const uint8_t *mask, size_t &used);
#endif

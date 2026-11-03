#include "FirmwareState.h"
#include "PixelItTransport.h"

void ServiceTransportReceive()
{
    ServiceDisplayFrames();
    yield();
}

void RecordSocketWriteError(uint8_t client, unsigned status, int error)
{
#if defined(ESP32)
    ++socketWriteStats.failures;
    socketWriteStats.client = client;
    socketWriteStats.status = status;
    socketWriteStats.error = error;
#endif
}

bool MqttReceiveAccepted()
{
#if defined(ESP8266) || defined(ESP32)
    return transportReceiveStats[0].status == 200;
#else
    return true;
#endif
}

void RecordTransportReceive(bool mqtt, unsigned status, size_t input, size_t output)
{
#if defined(ESP8266) || defined(ESP32)
    TransportReceiveStats &stats = transportReceiveStats[mqtt ? 0 : 1];
    ++stats.messages;
    stats.status = status;
    stats.inputBytes = input;
    stats.compactBytes = output;
    stats.freeHeap = ESP.getFreeHeap();
#if defined(ESP32)
    stats.maxBlock = ESP.getMaxAllocHeap();
#else
    stats.maxBlock = ESP.getMaxFreeBlockSize();
#endif
#endif
}

void NotifySocketReceiveError(uint8_t num, unsigned status)
{
#if defined(ESP32)
    const char *oversized = "{\"response\":\"WebSocket command exceeds device wire limit\"}";
#else
    const char *oversized = "{\"response\":\"WebSocket command exceeds 12288 bytes\"}";
#endif
    webSocket.sendTXT(num, status == 503 ? "{\"response\":\"Insufficient memory for WebSocket command\"}" :
                           status == 413 ? oversized :
                                           "{\"response\":\"Invalid or incomplete WebSocket command\"}");
}

#if defined(ESP8266) || defined(ESP32)
unsigned ReadCompactSocket(WiFiClient &client, uint8_t *output, size_t capacity,
                           size_t length, const uint8_t *mask, size_t &used)
{
    CompactTransportBuffer buffer(output, capacity);
    if (length > MAX_SCREEN_MESSAGE_SIZE) buffer.status = 413;
    else if (!output) buffer.status = 503;
    const unsigned long started = millis();
    unsigned long progress = started;
    size_t received = 0;
    while (received < length)
    {
        if (millis() - progress > 5000 || millis() - started > 10000 ||
            (!client.available() && !client.connected()))
        { buffer.status = 408; break; }
#if defined(ESP32)
        uint8_t input[128];
        const int available = client.available();
        size_t wanted = available > 128 ? 128 : available;
        if (wanted > length - received) wanted = length - received;
        const int count = wanted ? client.read(input, wanted) : 0;
        for (int i = 0; i < count; ++i)
        {
            int c = input[i];
#else
        for (unsigned batch = 0; batch < 128 && received < length && client.available(); ++batch)
        {
            int c = client.read();
#endif
            if (c < 0) break;
            if (mask) c ^= mask[received % 4];
            buffer.feed(static_cast<char>(c));
            ++received;
            progress = millis();
        }
        ServiceTransportReceive();
    }
    used = buffer.size;
    const unsigned status = buffer.finish();
    // Browser heartbeats are not commands and must not erase upload diagnostics.
    if (buffer.jsonCommand() || status != 200)
        RecordTransportReceive(false, status, length, used);
    return status;
}
#endif

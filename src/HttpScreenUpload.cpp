#include "FirmwareState.h"
#include "PixelItHttpBody.h"

#if defined(ESP8266) || defined(ESP32)
void SampleHttpUploadMemory()
{
    const uint32_t heap = ESP.getFreeHeap();
#if defined(ESP32)
    const uint32_t block = ESP.getMaxAllocHeap();
#else
    const uint32_t block = ESP.getMaxFreeBlockSize();
#endif
    if (!httpUploadStats.minHeap || heap < httpUploadStats.minHeap) httpUploadStats.minHeap = heap;
    if (!httpUploadStats.minBlock || block < httpUploadStats.minBlock) httpUploadStats.minBlock = block;
}

namespace
{
struct BodySink
{
    String &body;
    bool outOfMemory = false;
    explicit BodySink(String &body) : body(body) {}
    bool put(char c)
    {
        if (body.length() >= MAX_SCREEN_MESSAGE_SIZE || !body.concat(c))
        {
            outOfMemory = true;
            return false;
        }
        return true;
    }
};
}

int ReadScreenHttpBody(WiFiClient &client, uint32_t length, String &body, unsigned long timeout)
{
    httpUploadStats = HttpUploadStats();
    httpUploadStats.inputBytes = length;
    SampleHttpUploadMemory();
    int status = length > MAX_SCREEN_MESSAGE_SIZE ? 413 : length == 0 ? 400 : 200;
    // A 28x64 or 7x256 animation needs at most 5376 encoded pixel bytes plus
    // metadata. Reserve once in the smaller heap block, not the full wire size.
    // Small ordinary commands keep a small allocation. Encoding can add at
    // most two bytes per (valid, <=28) one-pixel frame; reserve that slack too.
#if defined(ESP32)
    const size_t capacity = length > SCREEN_RECEIVE_CAPACITY - 256 ? SCREEN_RECEIVE_CAPACITY : length + 256;
#else
    const size_t capacity = length > 6080 ? 6144 : length + 64;
#endif
    if (status == 200 && !body.reserve(capacity)) status = 503;
    SampleHttpUploadMemory();
    BodySink sink(body);
    ScreenJsonCompactor<BodySink> compact(sink);
    uint32_t received = 0;
    const unsigned long started = millis();
    unsigned long progress = started;
    while (received < length)
    {
        if (millis() - progress >= timeout || millis() - started >= timeout * 2)
        {
            status = 408;
            break;
        }
        const int available = client.available();
        if (!available && !client.connected()) { status = 400; break; }
        // Bounded CPU work before rendering again. Also drain rejected bodies
        // so a normal client receives the JSON error rather than a TCP reset.
        const unsigned batch = available > 128 ? 128 : available;
#if defined(ESP32)
        uint8_t input[128];
        const unsigned wanted = length - received < batch ? length - received : batch;
        const int count = wanted ? client.read(input, wanted) : 0;
        for (int i = 0; i < count; ++i)
        {
            const int c = input[i];
#else
        for (unsigned i = 0; i < batch && received < length; ++i)
        {
            const int c = client.read();
#endif
            if (c < 0) break;
            ++received;
            progress = millis();
            if (status == 200 && !compact.put(static_cast<char>(c)))
            {
                status = sink.outOfMemory ? 503 : 406;
                body = static_cast<const char *>(nullptr);
            }
        }
        SampleHttpUploadMemory();
        ServiceDisplayFrames();
        yield();
    }
    if (status == 200 && !compact.complete()) status = 406;
    httpUploadStats.compactBytes = body.length();
    httpUploadStats.status = status;
    if (status != 200) body = static_cast<const char *>(nullptr);
    return status;
}

const char *ScreenHttpError(int status)
{
    switch (status)
    {
#if defined(ESP32)
    case 413: return "{\"response\":\"Screen payload exceeds device wire limit\"}";
#else
    case 413: return "{\"response\":\"Screen payload exceeds 12288 bytes\"}";
#endif
    case 503: return "{\"response\":\"Insufficient memory for screen payload\"}";
    case 408: return "{\"response\":\"Screen upload timed out\"}";
    case 406: return "{\"response\":\"Invalid animation data\"}";
    default: return "{\"response\":\"Incomplete or missing request body\"}";
    }
}
#endif

#ifndef PIXELIT_WEBSOCKETS_H
#define PIXELIT_WEBSOCKETS_H

#include <WebSocketsServer.h>
#include <cstring>
#include "PixelItTransport.h"

class PixelItWebSockets : public WebSocketsServer
{
public:
    explicit PixelItWebSockets(uint16_t port) : WebSocketsServer(port) {}

    void beginPreview(const char *data, size_t length)
    {
        _previewData = data;
        _previewLength = length;
        memset(_previewOffset, 0, sizeof(_previewOffset));
    }

    void resetPreview(uint8_t num)
    {
        if (num < WEBSOCKETS_SERVER_CLIENT_MAX)
        {
            _previewOffset[num] = 0;
#if defined(ESP32)
            _previewSkipped[num] = _clients[num].status == WSC_CONNECTED;
#endif
        }
    }

    bool sendTXT(uint8_t num, const char *data, size_t length = 0)
    {
#if defined(ESP8266)
        // A data message cannot interrupt a fragmented WebSocket message.
        // Finish the preview before acknowledgements or status messages.
        if (num < WEBSOCKETS_SERVER_CLIENT_MAX && _previewOffset[num] > 0 &&
            _previewOffset[num] < _previewLength && _clients[num].tcp)
        {
            const size_t offset = _previewOffset[num];
            if (!sendFrame(&_clients[num], WSop_continuation,
                           reinterpret_cast<uint8_t *>(const_cast<char *>(_previewData + offset)),
                           _previewLength - offset, true))
            {
                disconnect(num);
                return false;
            }
            _previewOffset[num] = _previewLength;
        }
#endif
        return WebSocketsServer::sendTXT(num, data, length);
    }

    bool sendTXT(uint8_t num, String &data) { return sendTXT(num, data.c_str(), data.length()); }

    bool sendPreview(uint8_t num, const char *data, size_t length)
    {
#if defined(ESP8266)
        if (num >= WEBSOCKETS_SERVER_CLIENT_MAX || !_clients[num].tcp ||
            _clients[num].status != WSC_CONNECTED)
            return false;
        size_t &offset = _previewOffset[num];
        if (offset >= length)
            return true;
        const int room = _clients[num].tcp->availableForWrite();
        if (room <= 4)
            return false;
        size_t chunk = length - offset;
        if (chunk > 1024)
            chunk = 1024;
        if (chunk > static_cast<size_t>(room - 4))
            chunk = room - 4;
        // WebSocket fragments form one unchanged JSON message in the browser.
        // Queue only what fits now; continue in a later loop without waiting.
        const bool last = offset + chunk == length;
        if (!sendFrame(&_clients[num], offset ? WSop_continuation : WSop_text,
                       reinterpret_cast<uint8_t *>(const_cast<char *>(data + offset)), chunk, last))
        {
            disconnect(num);
            return true;
        }
        offset += chunk;
        return last;
#else
        if (num >= WEBSOCKETS_SERVER_CLIENT_MAX) return false;
        _previewSkipped[num] = true;
        if (!SocketPreviewReady(num) || !previewTurn(num)) return true;
        const bool sent = SendSocketPreview(num, data, length);
        if (sent)
        {
            _previewSkipped[num] = false;
            _nextPreviewClient = (num + 1) % WEBSOCKETS_SERVER_CLIENT_MAX;
        }
        return sent;
#endif
    }

#if defined(ESP32)
    void flushSkippedPreviews()
    {
        // The global Liveview buffer always holds the latest sample. Copy it
        // into a recovered viewer's empty output ring, including a static final
        // image whose checksum no longer changes. No per-viewer pixel copy.
        if (!_previewData || !_previewLength) return;
        for (uint8_t num = 0; num < WEBSOCKETS_SERVER_CLIENT_MAX; ++num)
            if (_previewSkipped[num] && SocketPreviewReady(num) && clientIsConnected(num))
                sendPreview(num, _previewData, _previewLength);
    }
#endif

private:
#if defined(ESP32)
    bool previewTurn(uint8_t num)
    {
        // Only ready/ACK-drained viewers can take a turn. There is no fixed
        // FPS ceiling; unsent samples are replaceable and never build a backlog.
        for (uint8_t offset = 0; offset < WEBSOCKETS_SERVER_CLIENT_MAX; ++offset)
        {
            const uint8_t next = (_nextPreviewClient + offset) % WEBSOCKETS_SERVER_CLIENT_MAX;
            if (_previewSkipped[next] && SocketPreviewReady(next) && clientIsConnected(next))
                return next == num;
        }
        return false;
    }
    uint8_t _nextPreviewClient = 0;
    bool _previewSkipped[WEBSOCKETS_SERVER_CLIENT_MAX] = {};
#endif
    size_t _previewOffset[WEBSOCKETS_SERVER_CLIENT_MAX] = {};
    const char *_previewData = nullptr;
    size_t _previewLength = 0;
};

#endif

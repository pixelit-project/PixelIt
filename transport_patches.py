"""Guarded source transformations for the pinned PubSubClient/WebSockets versions.
Only generated build copies are changed, never installed dependency sources.
"""

def once(source, old, new):
    if source.count(old) != 1:
        raise RuntimeError('Review pinned transport patch: ' + old[:75])
    return source.replace(old, new)


def patch_mqtt(source):
    source = '#include "PixelItTransport.h"\n' + source
    source = once(source, '   while(!_client->available()) {',
                  '   while(!_client->available()) {\n     ServiceTransportReceive();')
    marker = '    uint32_t idx = len;'
    receiver = r'''
    if (isPublish && !this->stream) {
        // Topic/message ID stay at their original offsets. Only JSON payload
        // bytes are compacted, in place in the existing bounded client buffer.
        const uint32_t payloadLength = length >= 2 + skip ? length - 2 - skip : 0;
        bool metadataOk = length >= 2 + skip && len + skip + 3 < this->bufferSize;
        for (uint32_t i = 0; i < skip && i + 2 < length; ++i) {
            if (!readByte(&digit)) return 0;
            if (metadataOk) this->buffer[len++] = digit;
            if ((i & 127) == 127) ServiceTransportReceive();
        }
        const size_t topicLength = (this->buffer[*lengthLength + 1] << 8) + this->buffer[*lengthLength + 2];
        const uint8_t *topicStart = this->buffer + *lengthLength + 3;
        const bool screenTopic = metadataOk && topicLength >= 9 &&
            !memcmp(topicStart + topicLength - 9, "setScreen", 9) &&
            (topicLength == 9 || topicStart[topicLength - 10] == '/');
        CompactTransportBuffer compact(metadataOk ? this->buffer + len : nullptr,
                                       metadataOk ? this->bufferSize - len - 1 : 0, screenTopic);
        if (!metadataOk) compact.status = 503;
        else if (length + *lengthLength + 1 > 11264) compact.status = 413;
        const unsigned long receiveStarted = millis();
        for (uint32_t i = 0; i < payloadLength; ++i) {
            if (!readByte(&digit)) { _client->stop(); return 0; }
            compact.feed(static_cast<char>(digit));
            if ((i & 127) == 127) ServiceTransportReceive();
            if (millis() - receiveStarted > 10000) { _client->stop(); return 0; }
        }
        const unsigned status = compact.finish();
        RecordTransportReceive(true, status, payloadLength, compact.size);
        if (!metadataOk) return 0;
        if (status != 200) {
            // Keep the packet metadata so QoS1 can still be acknowledged, but
            // the application callback is explicitly suppressed on rejection.
            this->buffer[len++] = '{'; this->buffer[len++] = '}';
        } else len += compact.size;
        this->buffer[len] = 0;
        return len;
    }
'''
    source = once(source, marker, receiver + marker)
    for suffix in ('-2', ''):
        old = 'callback(topic,payload,len-llen-3-tl' + suffix + ');'
        source = once(source, old, 'if (MqttReceiveAccepted()) ' + old)
    return source


def patch_websocket(source):
    source = '#include "PixelItTransport.h"\n' + source
    marker = '    if(header->payloadLen > 0) {\n        // if text data we need one more'
    receiver = r'''
    if (header->opCode == WSop_text && header->fin) {
        const size_t capacity = header->payloadLen > 6080 ? 6144 : header->payloadLen + 64;
        payload = header->payloadLen <= 12288 ? (uint8_t *)malloc(capacity + 1) : nullptr;
        size_t used = 0;
        const unsigned status = ReadCompactSocket(*client->tcp, payload, capacity,
            header->payloadLen, header->mask ? header->maskKey : nullptr, used);
        if (status == 200) {
            payload[used] = 0;
            messageReceived(client, WSop_text, payload, used, true);
        } else NotifySocketReceiveError(client->num, status);
        free(payload);
        client->cWsRXsize = 0;
        if (status == 408) clientDisconnect(client, 1002);
        return;
    }
'''
    source = once(source, marker, receiver + marker)
    # Header reads and outgoing acknowledgements/previews must also keep LED
    # deadlines while TCP is waiting. Never ask write() for more than fits now.
    if source.count('    while(n > 0) {') != 2:
        raise RuntimeError('Review pinned WebSocket read/write loops')
    source = source.replace('    while(n > 0) {', '    while(n > 0) {\n        ServiceTransportReceive();')
    return once(source, '        len = client->tcp->write((const uint8_t *)out, n);', r'''
        const int room = client->tcp->availableForWrite();
        size_t chunk = n > 512 ? 512 : n;
        if (room > 0 && chunk > static_cast<size_t>(room)) chunk = room;
        len = room > 0 ? client->tcp->write((const uint8_t *)out, chunk) : 0;
''')

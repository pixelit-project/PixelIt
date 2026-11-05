#include "FirmwareState.h"
#include "PixelItTransport.h"

#if defined(ESP32)
#include <lwip/sockets.h>
#include <errno.h>

static_assert(WEBSOCKETS_SERVER_CLIENT_MAX == 5, "Review bounded ESP32 output queue budget");

void ResetSocketOutput(uint8_t client, bool enabled)
{
    if (client < 5)
    {
        socketOutput[client].reset();
        socketOutput[client].enabled = enabled;
    }
}

bool SocketOutputEnabled(uint8_t client)
{
    return client < 5 && socketOutput[client].enabled;
}

bool SocketPreviewReady(uint8_t client)
{
    // Preview samples are replaceable; status and acknowledgements take priority.
    return client < 5 && socketOutput[client].previewEnabled && !socketOutput[client].failed && socketOutput[client].initialStep == 0 &&
           socketOutput[client].transportReady && socketOutput[client].size() == 0;
}

static void FlushSocketClient(uint8_t client);

size_t QueueSocketWrite(uint8_t client, int fd, const uint8_t *data, size_t length)
{
    if (client >= 5 || fd < 0) return 0;
    SocketOutputQueue &queue = socketOutput[client];
    // Keep a failed established client on the nonblocking path until the
    // outer loop retires it. sendFrame may still try its payload after failure.
    if (queue.failed) return 0;
    if (queue.fd >= 0 && queue.fd != fd) queue.reset();
    queue.fd = fd;
    queue.enabled = true;
    // Initial state can arrive in a burst larger than the ring. Make bounded
    // nonblocking progress before rejecting; never increase the allocation.
    for (unsigned attempt = 0; attempt < 3 && length <= SocketOutputQueue::capacity &&
         length > SocketOutputQueue::capacity - queue.size(); ++attempt)
    {
        const size_t before = queue.size();
        FlushSocketClient(client);
        if (queue.fd != fd) return 0;
        if (queue.size() == before) break;
        ServiceTransportReceive();
    }
    const bool empty = queue.size() == 0;
    if (!queue.append(data, length))
    {
        RecordSocketWriteError(client, 503, ENOBUFS);
        ::shutdown(fd, SHUT_RDWR);
        queue.reset();
        queue.enabled = true;
        queue.failed = true;
        return 0;
    }
    ++queue.writes;
    queue.transportReady = false;
    if (empty) queue.progress = millis();
    return length;
}

static void FlushSocketClient(uint8_t client)
{
    SocketOutputQueue &queue = socketOutput[client];
    if (!queue.size()) return;
    const size_t length = queue.contiguous() > 1460 ? 1460 : queue.contiguous();
    const int sent = ::send(queue.fd, queue.data(), length, MSG_DONTWAIT);
    if (sent > 0)
    {
        ++queue.writes;
        queue.transportReady = false;
        queue.consume(static_cast<size_t>(sent));
        queue.progress = millis();
    }
    else if ((sent < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) ||
             millis() - queue.progress > 5000)
    {
        RecordSocketWriteError(client, 408, sent < 0 ? errno : 0);
        ::shutdown(queue.fd, SHUT_RDWR);
        queue.reset();
        queue.enabled = true;
        queue.failed = true;
    }
}

void FlushSocketOutput()
{
    ServiceSocketTransportProbe();
    for (uint8_t client = 0; client < 5; ++client)
    {
        FlushSocketClient(client);
        // Outside sendFrame/QueueSocketWrite: disconnect callbacks may emit logs.
        // Reject their writes to this failed client until native cleanup completes.
        if (socketOutput[client].failed)
            webSocket.disconnect(client);
        ServiceTransportReceive();
    }
}
#endif

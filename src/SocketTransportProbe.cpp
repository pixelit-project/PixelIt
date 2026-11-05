#include "FirmwareState.h"

#if defined(ESP32)
#include <lwip/tcpip.h>
#include <lwip/tcp.h>
#include <lwip/api.h>
#include <freertos/semphr.h>
extern "C" {
#include <lwip/priv/sockets_priv.h>
}
#if !ESP_LWIP_LOCK || !LWIP_NETCONN_FULLDUPLEX
#error Review socket lifetime protection for this ESP32 framework
#endif

namespace
{
void ProbeSocketTransport(void *)
{
    // Runs only on the lwIP core, where socket/PCB access is serialized with
    // native sends and closes. No network work, allocations or logging here.
    bool ready[5] = {};
    for (uint8_t n = 0; n < 5; ++n)
    {
        const int fd = socketTransportProbe.fd[n];
        if (fd < 0) continue;
        lwip_sock *socket = lwip_socket_dbg_get_socket(fd);
        // The pinned port keeps this per-slot mutex allocated for its lifetime.
        // A zero-wait take protects conn lifetime against native close/free;
        // PCB fields themselves are read only here on the lwIP core.
        if (!socket || !socket->lock || xSemaphoreTake(socket->lock, 0) != pdTRUE)
            continue;
        if (socket->conn && NETCONNTYPE_GROUP(socket->conn->type) == NETCONN_TCP)
        {
            const tcp_pcb *pcb = socket->conn->pcb.tcp;
            if (pcb && pcb->state == ESTABLISHED && !pcb->unsent)
            {
                constexpr size_t websocketHeader = 4;
                constexpr size_t compactFrame = compactLiveviewLength + websocketHeader;
                if (socketTransportProbe.compact[n])
                {
                    // Bound in-flight data to two compact frames in total.
                    const uint32_t inFlight = pcb->snd_lbb - pcb->lastack;
                    ready[n] = inFlight <= compactFrame && tcp_sndbuf(pcb) >= compactFrame;
                }
                else
                {
                    ready[n] = !pcb->unacked && tcp_sndbuf(pcb) >= (_LIVEVIEW_BUFFER_LENGHT + websocketHeader);
                }
            }
        }
        xSemaphoreGive(socket->lock);
    }
    portENTER_CRITICAL(&socketTransportProbeMux);
    for (uint8_t n = 0; n < 5; ++n) socketTransportProbe.ready[n] = ready[n];
    socketTransportProbe.result = true;
    socketTransportProbe.pending = false;
    portEXIT_CRITICAL(&socketTransportProbeMux);
}
}

void ServiceSocketTransportProbe()
{
    const uint32_t now = millis();
    // One reusable callback message; retry allocation at most once per second.
    if (!socketTransportProbe.message)
    {
        if (socketTransportProbe.allocationAttempted &&
            now - socketTransportProbe.lastAllocationAttempt < 1000) return;
        socketTransportProbe.allocationAttempted = true;
        socketTransportProbe.lastAllocationAttempt = now;
        socketTransportProbe.message = tcpip_callbackmsg_new(ProbeSocketTransport, nullptr);
        if (!socketTransportProbe.message) return;
    }
    portENTER_CRITICAL(&socketTransportProbeMux);
    if (socketTransportProbe.pending)
    {
        portEXIT_CRITICAL(&socketTransportProbeMux);
        return;
    }
    if (socketTransportProbe.result)
    {
        for (uint8_t n = 0; n < 5; ++n)
        {
            SocketOutputQueue &queue = socketOutput[n];
            if (queue.fd == socketTransportProbe.fd[n] &&
                queue.epoch == socketTransportProbe.epoch[n] &&
                queue.writes == socketTransportProbe.writes[n] &&
                queue.compactPreview == socketTransportProbe.compact[n])
                queue.transportReady = socketTransportProbe.ready[n];
        }
        socketTransportProbe.result = false;
    }
    if (static_cast<uint32_t>(now - socketTransportProbe.lastPoll) < 20)
    {
        portEXIT_CRITICAL(&socketTransportProbeMux);
        return;
    }
    socketTransportProbe.lastPoll = now;
    for (uint8_t n = 0; n < 5; ++n)
    {
        socketTransportProbe.fd[n] = socketOutput[n].fd;
        socketTransportProbe.epoch[n] = socketOutput[n].epoch;
        socketTransportProbe.writes[n] = socketOutput[n].writes;
        socketTransportProbe.compact[n] = socketOutput[n].compactPreview;
    }
    socketTransportProbe.pending = true;
    portEXIT_CRITICAL(&socketTransportProbeMux);
    // Never wait for the core thread or enqueue a second outstanding probe.
    if (tcpip_callbackmsg_trycallback(socketTransportProbe.message) != ERR_OK)
    {
        portENTER_CRITICAL(&socketTransportProbeMux);
        socketTransportProbe.pending = false;
        portEXIT_CRITICAL(&socketTransportProbeMux);
    }
}
#endif

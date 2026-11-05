#pragma once
#include <stdint.h>

struct tcpip_callback_msg;
// Main-loop snapshots and results shared with the lwIP core callback. Never
// retain PCB/socket pointers; fd reuse is checked with connection/write epochs.
struct SocketTransportProbe
{
    tcpip_callback_msg *message = nullptr;
    uint32_t lastPoll = 0, lastAllocationAttempt = 0;
    int fd[5] = {-1, -1, -1, -1, -1};
    uint32_t epoch[5] = {}, writes[5] = {};
    bool ready[5] = {}, compact[5] = {};
    bool pending = false, result = false, allocationAttempted = false;
};

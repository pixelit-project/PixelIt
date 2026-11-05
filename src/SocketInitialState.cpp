#include "FirmwareState.h"

#if defined(ESP32)
void ServiceSocketInitialState()
{
    // One message total per loop, built only after the previous output drains.
    // No retained JSON or second queue; slow clients cannot accumulate snapshots.
    for (uint8_t num = 0; num < 5; ++num)
    {
        SocketOutputQueue &queue = socketOutput[num];
        if (!queue.initialStep || queue.failed || queue.size() ||
            !webSocket.clientIsConnected(num)) continue;
        String payload;
        switch (queue.initialStep)
        {
        case 1: payload = "{\"sysinfo\":" + GetMatrixInfo() + "}"; break;
        case 2: payload = "{\"sensor\":" + GetLuxSensor() + "}"; break;
        case 3: payload = "{\"sensor\":" + GetSensor() + "}"; break;
        case 4: payload = "{\"config\":" + GetConfig() + "}"; break;
        case 5: payload = "{\"buttons\":" + GetButtons() + "}"; break;
        case 6: payload = "{\"telemetry\":" + GetTelemetry() + "}"; break;
        default: queue.initialStep = 0; continue;
        }
        if (webSocket.sendTXT(num, payload.c_str(), payload.length()))
            queue.initialStep = queue.initialStep == 6 ? 0 : queue.initialStep + 1;
        return;
    }
}
#endif

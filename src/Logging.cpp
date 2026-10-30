#include "FirmwareState.h"
#include "Tools.h"

void Log(String function, String message)
{

    String timeStamp = IntFormat(year()) + "-" + IntFormat(month()) + "-" + IntFormat(day()) + "T" + IntFormat(hour()) + ":" + IntFormat(minute()) + ":" + IntFormat(second());

    Serial.println("[" + timeStamp + "] " + function + ": " + message);

    // Prüfen ob über Websocket versendet werden muss
    if (webSocket.connectedClients() > 0)
    {
        for (unsigned int i = 0; i < sizeof websocketConnection / sizeof websocketConnection[0]; i++)
        {
            String payload = "{\"log\":{\"timeStamp\":\"" + timeStamp + "\",\"function\":\"" + function + "\",\"message\":\"" + message + "\"}}";
            webSocket.sendTXT(i, payload);
        }
    }
}
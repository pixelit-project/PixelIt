#include "FirmwareState.h"
#include "Tools.h"

void SendMatrixInfo()
{
    // Check if mqtt or websocket connected
    if ((mqttAktiv == true && client.connected()) || (webSocket.connectedClients() > 0))
    {
        String matrixInfo = GetMatrixInfo();

        // Check if sending via MQTT is required
        if (mqttAktiv == true && client.connected())
        {
            PublishMqtt((mqttMasterTopic + "matrixinfo").c_str(), matrixInfo.c_str(), true);
            if (mqttUseDeviceTopic)
            {
                PublishMqtt((mqttDeviceTopic + "matrixinfo").c_str(), matrixInfo.c_str(), true);
            }
        }
        // Check if sending via websocket is required
        if (webSocket.connectedClients() > 0)
        {
            for (uint i = 0; i < sizeof websocketConnection / sizeof websocketConnection[0]; i++)
            {
                SendWebSocketJson(i, "{\"sysinfo\":" + matrixInfo + "}");
            }
        }
    }
}

void SendLDR(bool force)
{
    if (force)
    {
        oldGetLuxSensor = "";
    }

    String luxSensor;

    // Prüfen ob die Abfrage des LuxSensor überhaupt erforderlich ist
    if ((mqttAktiv == true && client.connected()) || (webSocket.connectedClients() > 0))
    {
        luxSensor = GetLuxSensor();
    }
    // Prüfen ob über MQTT versendet werden muss
    if (mqttAktiv == true && client.connected() && oldGetLuxSensor != luxSensor)
    {
        PublishMqtt((mqttMasterTopic + "luxsensor").c_str(), luxSensor.c_str(), true);
        if (mqttUseDeviceTopic)
        {
            PublishMqtt((mqttDeviceTopic + "luxsensor").c_str(), luxSensor.c_str(), true);
        }
    }
    // Prüfen ob über Websocket versendet werden muss
    if (webSocket.connectedClients() > 0 && oldGetLuxSensor != luxSensor)
    {
        for (unsigned int i = 0; i < sizeof websocketConnection / sizeof websocketConnection[0]; i++)
        {
            SendWebSocketJson(i, "{\"sensor\":" + luxSensor + "}");
        }
    }

    oldGetLuxSensor = luxSensor;
}
bool sendLiveview(const char *data, size_t length, bool retry)
{
    if (!retry)
        webSocket.beginPreview(data, length);
    bool delivered = true;
    if (webSocket.connectedClients() > 0)
    {
        for (unsigned int i = 0; i < sizeof websocketConnection / sizeof websocketConnection[0]; i++)
        {
            if (webSocket.clientIsConnected(i) && !webSocket.sendPreview(i, data, length))
                delivered = false;
        }
    }
    return delivered;
}

void SendSensor(bool force)
{
    if (force)
    {
        oldGetSensor = "";
    }

    String Sensor;

    // Prüfen ob die Abfrage des LuxSensor überhaupt erforderlich ist
    if ((mqttAktiv == true && client.connected()) || (webSocket.connectedClients() > 0))
    {
        Sensor = GetSensor();
    }
    // Prüfen ob über MQTT versendet werden muss
    if (mqttAktiv == true && client.connected() && oldGetSensor != Sensor)
    {
        PublishMqtt((mqttMasterTopic + "dhtsensor").c_str(), Sensor.c_str(), true); // Legancy
        PublishMqtt((mqttMasterTopic + "sensor").c_str(), Sensor.c_str(), true);
        if (mqttUseDeviceTopic)
        {
            PublishMqtt((mqttDeviceTopic + "dhtsensor").c_str(), Sensor.c_str(), true); // Legancy
            PublishMqtt((mqttDeviceTopic + "sensor").c_str(), Sensor.c_str(), true);
        }
    }
    // Prüfen ob über Websocket versendet werden muss
    if (webSocket.connectedClients() > 0 && oldGetSensor != Sensor)
    {
        for (uint i = 0; i < sizeof websocketConnection / sizeof websocketConnection[0]; i++)
        {
            SendWebSocketJson(i, "{\"sensor\":" + Sensor + "}");
        }
    }

    oldGetSensor = Sensor;
}

void SendConfig()
{
    if (webSocket.connectedClients() > 0)
    {
        for (uint i = 0; i < sizeof websocketConnection / sizeof websocketConnection[0]; i++)
        {
            String config = GetConfig();
            SendWebSocketJson(i, "{\"config\":" + config + "}");
        }
    }
}


#include "FirmwareState.h"
#include "Tools.h"

/////////////////////////////////////////////////////////////////////
/*-------- NTP code ----------*/
const int NTP_PACKET_SIZE = 48;
byte packetBuffer[NTP_PACKET_SIZE];

time_t getNtpTime()
{
    while (udp.parsePacket() > 0)
        ;
    sendNTPpacket(ntpServer);
    uint32_t beginWait = millis();
    while (millis() - beginWait < 1500)
    {
        int size = udp.parsePacket();
        if (size >= NTP_PACKET_SIZE)
        {
            udp.read(packetBuffer, NTP_PACKET_SIZE);
            time_t secsSince1900;

            secsSince1900 = (time_t)packetBuffer[40] << 24;
            secsSince1900 |= (time_t)packetBuffer[41] << 16;
            secsSince1900 |= (time_t)packetBuffer[42] << 8;
            secsSince1900 |= (time_t)packetBuffer[43];
            time_t secsSince1970 = secsSince1900 - 2208988800UL;
            float totalOffset = clockTimeZone;
            if (clockDayLightSaving)
            {
                totalOffset = (clockTimeZone + DSToffset(secsSince1970, clockTimeZone));
            }
            return secsSince1970 + (time_t)(totalOffset * SECS_PER_HOUR);
            ntpRetryCounter = 0;
        }
        yield();
    }
    ntpRetryCounter++;
    return 0;
}
void sendNTPpacket(String &address)
{
    memset(packetBuffer, 0, NTP_PACKET_SIZE);

    packetBuffer[0] = 0b11100011;
    packetBuffer[1] = 0;
    packetBuffer[2] = 6;
    packetBuffer[3] = 0xEC;

    packetBuffer[12] = 49;
    packetBuffer[13] = 0x4E;
    packetBuffer[14] = 49;
    packetBuffer[15] = 52;

    udp.beginPacket(address.c_str(), 123);
    udp.write(packetBuffer, NTP_PACKET_SIZE);
    udp.endPacket();
}


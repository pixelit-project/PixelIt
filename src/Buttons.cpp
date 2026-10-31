#include "FirmwareState.h"
#include "Tools.h"

void PublishButtonEvent(uint button, const char *event)
{
    if (!mqttAktiv || !mqttHAdiscoverable || !client.connected()) return;
    const String payload = "{\"event_type\":\"" + String(event) + "\"}";
    const String suffix = "buttons/button" + String(button) + "/event";
    // Events must never be retained: reconnecting must not replay a press.
    PublishMqtt((mqttMasterTopic + suffix).c_str(), payload.c_str());
    if (mqttUseDeviceTopic) PublishMqtt((mqttDeviceTopic + suffix).c_str(), payload.c_str());
}

void ServiceButtons()
{
    struct ButtonState
    {
        bool raw = false;
        bool pressed = false;
        bool longPress = false;
        bool pendingClick = false;
        unsigned long changedAt = 0;
        unsigned long pressedAt = 0;
        unsigned long releasedAt = 0;
    };
    static ButtonState states[3];
    const unsigned long now = millis();
    for (uint button = 0; button < 3; ++button)
    {
        ButtonState &s = states[button];
        if (!btnEnabled[button])
        {
            s = ButtonState();
            continue;
        }
        const bool raw = digitalRead(TranslatePin(btnPin[button])) == btnPressedLevel[button];
        if (raw != s.raw)
        {
            s.raw = raw;
            s.changedAt = now;
        }
        if (raw != s.pressed && now - s.changedAt >= 30)
        {
            s.pressed = raw;
            HandleAndSendButtonPress(button, raw);
            if (raw)
            {
                s.pressedAt = now;
                s.longPress = false;
            }
            else if (!s.longPress)
            {
                if (s.pendingClick && now - s.releasedAt <= 350)
                {
                    PublishButtonEvent(button, "double_press");
                    s.pendingClick = false;
                }
                else
                {
                    if (s.pendingClick) PublishButtonEvent(button, "single_press");
                    s.pendingClick = true;
                    s.releasedAt = now;
                }
            }
        }
        if (s.pressed && !s.longPress && now - s.pressedAt >= 800)
        {
            s.longPress = true;
            PublishButtonEvent(button, "long_press");
        }
        if (s.pendingClick && now - s.releasedAt > 350)
        {
            PublishButtonEvent(button, "single_press");
            s.pendingClick = false;
        }
    }
}

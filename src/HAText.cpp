#include "FirmwareState.h"
#include "Tools.h"

namespace
{
String lastNonBlackTextColor = "#FFFFFF";
String pendingHATextTopic, pendingHATextPayload;
bool haTextPending = false;
bool IsTextTopic(const char *topic, const char *command)
{
    return String(topic) == mqttMasterTopic + command ||
           (mqttUseDeviceTopic && String(topic) == mqttDeviceTopic + command);
}

bool ValidTextColor(const String &color)
{
    if (color.length() != 7 || color[0] != '#') return false;
    for (uint i = 1; i < 7; ++i)
    {
        const char c = color[i];
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'))) return false;
    }
    return true;
}

bool ApplyTextOptions(JsonObject &options)
{
    // Validate the complete update before applying any part of it.
    if (options.containsKey("text") && (!options["text"].is<const char *>() || options["text"].as<String>().length() > 255)) return false;
    if (options.containsKey("color") && !ValidTextColor(options["color"].as<String>())) return false;
    if (options.containsKey("font") && options["font"] != "Small" && options["font"] != "Large") return false;
    if (options.containsKey("alignment") && options["alignment"] != "Centered" && options["alignment"] != "Left") return false;
    if (options.containsKey("scroll") && options["scroll"] != "Auto" && options["scroll"] != "Always" && options["scroll"] != "Off") return false;
    if (options.containsKey("text")) haTextMessage = options["text"].as<String>();
    if (options.containsKey("color"))
    {
        haTextColor = options["color"].as<String>();
        if (haTextColor != "#000000") lastNonBlackTextColor = haTextColor;
    }
    if (options.containsKey("font")) haTextFont = options["font"].as<String>();
    if (options.containsKey("alignment")) haTextAlignment = options["alignment"].as<String>();
    if (options.containsKey("scroll")) haTextScroll = options["scroll"].as<String>();
    return true;
}
}

void ShowHAText()
{
    DynamicJsonBuffer buffer;
    JsonObject &screen = buffer.createObject();
    screen["sleepMode"] = false;
    CreateFrames(screen);
    screen.remove("sleepMode");
    JsonObject &content = screen.createNestedObject("text");
    content["textString"] = haTextMessage;
    content["hexColor"] = haTextColor;
    const bool large = haTextFont == "Large";
    content["bigFont"] = large ? 1 : 0;
    content["centerText"] = haTextAlignment == "Centered";
    if (haTextScroll == "Auto") content["scrollText"] = "auto";
    else content["scrollText"] = haTextScroll == "Always";

    // Match DrawTextHelper's font-specific baseline correction.
    int16_t boundsX, boundsY;
    uint16_t textWidth, textHeight;
    if (large) matrix->setFont();
    else matrix->setFont(&PixelItFont);
    matrix->getTextBounds(Utf8ToAscii(haTextMessage), 0, 0, &boundsX, &boundsY, &textWidth, &textHeight);
    const int top = textHeight < MATRIX_HEIGHT ? (MATRIX_HEIGHT - textHeight) / 2 : 0;
    JsonObject &position = content.createNestedObject("position");
    position["y"] = textHeight ? top - boundsY - (large ? -1 : 5) : 0;
    CreateFrames(screen);
    SendMatrixInfo();
}

bool HandleHATextCommand(const char *topic, const byte *payload, unsigned int length)
{
    const bool immediate = IsTextTopic(topic, "setText");
    const bool options = IsTextTopic(topic, "setTextOptions");
    const bool picker = IsTextTopic(topic, "setTextOptions/colorPicker");
    const char *field = nullptr;
    const char *fields[] = {"text", "color", "font", "alignment", "scroll"};
    for (const char *candidate : fields)
        if (IsTextTopic(topic, (String("setTextOptions/") + candidate).c_str())) field = candidate;
    const bool show = IsTextTopic(topic, "showText");
    if (!immediate && !options && !show && !field && !picker) return false;
    if (show)
    {
        if (length == 5 && memcmp(payload, "PRESS", 5) == 0) ShowHAText();
        return true;
    }
    if (length > ((immediate || field || picker) ? 255U : 2048U)) return true;
    String body;
    body.reserve(length);
    for (uint i = 0; i < length; ++i)
    {
        if (payload[i] == 0) return true;
        body += static_cast<char>(payload[i]);
    }
    if (immediate)
    {
        haTextMessage = body;
        ShowHAText();
    }
    else if (picker)
    {
        if (body == "ON") body = lastNonBlackTextColor;
        else if (body == "OFF") body = "#000000";
        DynamicJsonBuffer buffer;
        JsonObject &update = buffer.createObject();
        update["color"] = body;
        if (ApplyTextOptions(update)) SendMatrixInfo();
    }
    else if (field)
    {
        DynamicJsonBuffer buffer;
        JsonObject &update = buffer.createObject();
        update[field] = body;
        if (ApplyTextOptions(update)) SendMatrixInfo();
    }
    else
    {
        DynamicJsonBuffer buffer;
        JsonObject &update = buffer.parseObject(body);
        if (update.success() && ApplyTextOptions(update)) SendMatrixInfo();
    }
    return true;
}

// PubSubClient holds an 11 KB receive buffer while invoking its callback.
// Only copy the command there; render and serialize states after releasing it.
bool QueueHATextCommand(const char *topic, const byte *payload, unsigned int length)
{
    bool known = IsTextTopic(topic, "setText") || IsTextTopic(topic, "setTextOptions") ||
                 IsTextTopic(topic, "showText") || IsTextTopic(topic, "setTextOptions/colorPicker");
    const char *fields[] = {"text", "color", "font", "alignment", "scroll"};
    for (const char *field : fields)
        known |= IsTextTopic(topic, (String("setTextOptions/") + field).c_str());
    if (!known) return false;
    if (length > 2048) return true;
    pendingHATextTopic = topic;
    pendingHATextPayload = "";
    if (!pendingHATextPayload.reserve(length)) return true;
    for (uint i = 0; i < length; ++i)
    {
        if (payload[i] == 0) return true;
        pendingHATextPayload += static_cast<char>(payload[i]);
    }
    haTextPending = true;
    return true;
}

void ServiceHAText()
{
    if (!haTextPending) return;
    haTextPending = false;
    HandleHATextCommand(pendingHATextTopic.c_str(), reinterpret_cast<const byte *>(pendingHATextPayload.c_str()), pendingHATextPayload.length());
    pendingHATextTopic = static_cast<const char *>(nullptr);
    pendingHATextPayload = static_cast<const char *>(nullptr);
}

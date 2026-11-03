#pragma once

#if defined(ESP8266)
#include <ESP8266WiFi.h>
#elif defined(ESP32)
#include <WiFi.h>
#endif

#if defined(ESP8266) || defined(ESP32)

// Used by the local pinned-core parser patch; does not change installed headers.
int ReadScreenHttpBody(WiFiClient &client, uint32_t length, String &body, unsigned long timeout);
const char *ScreenHttpError(int status);
void SampleHttpUploadMemory();
#endif

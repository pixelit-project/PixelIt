#pragma once
#include <stddef.h>
#include <stdint.h>

// Keep transport and animation budgets together; ESP8266 values are unchanged.
#if defined(ESP32)
static const size_t MAX_ANIMATION_FRAMES = 160;
static const size_t MAX_ANIMATION_PIXELS = 10240;
static const uint16_t MAX_SCREEN_MESSAGE_SIZE = 65535;
static const uint16_t MAX_MQTT_PACKET_SIZE = 65535;
static const uint16_t MQTT_BUFFER_SIZE = 11264;
static const size_t SCREEN_RECEIVE_CAPACITY = 32768;
#else
static const size_t MAX_ANIMATION_FRAMES = 28;
static const size_t MAX_ANIMATION_PIXELS = MAX_ANIMATION_FRAMES * 64;
// 28 x 64 RGB565 values need at most 10,809 JSON bytes, plus metadata/topic.
static const uint16_t MAX_SCREEN_MESSAGE_SIZE = 12288;
static const uint16_t MQTT_BUFFER_SIZE = 11264;
#endif
static const uint16_t MQTT_IDLE_BUFFER_SIZE = 256;


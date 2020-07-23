#pragma once
// This file is auto-generated! Do not edit!

#define USE_API
#define USE_LOGGER
#define USE_BINARY_SENSOR
#define USE_SENSOR
#define USE_SWITCH
#define USE_WIFI
#define USE_STATUS_LED
#define USE_TEXT_SENSOR
#define USE_FAN
#define USE_COVER
#define USE_LIGHT
#define USE_CLIMATE
#define USE_MQTT
#define USE_POWER_SUPPLY
#define USE_HOMEASSISTANT_TIME
#define USE_JSON
#ifdef ARDUINO_ARCH_ESP32
#define USE_ESP32_CAMERA
#endif
#define USE_TIME
#define USE_DEEP_SLEEP
#define USE_CAPTIVE_PORTAL

#ifdef ARDUINO_ARCH_ESP8266
#define USE_TCP_ESP8266_WIFI_CLIENT
#define USE_TCP_LWIP_RAW_TCP
#define USE_TCP_ASYNC_TCP
#elif ARDUINO_ARCH_ESP32
#define USE_TCP_ESP32_WIFI_CLIENT
#define USE_TCP_UNIX_SOCKET
#define USE_TCP_ASYNC_TCP
#else
#define USE_TCP_UNIX_SOCKET
#endif

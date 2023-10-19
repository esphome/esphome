#pragma once

// This file is not used by the runtime, instead, a version is generated during
// compilation with only the relevant feature flags for the current build.
//
// This file is only used by static analyzers and IDEs.

#include "esphome/core/macros.h"

// Informative flags
#define ESPHOME_BOARD "dummy_board"
#define ESPHOME_PROJECT_NAME "dummy project"
#define ESPHOME_PROJECT_VERSION "v2"
#define ESPHOME_VARIANT "ESP32"

// Feature flags
#define USE_API
#define USE_API_NOISE
#define USE_API_PLAINTEXT
#define USE_ALARM_CONTROL_PANEL
#define USE_BINARY_SENSOR
#define USE_BUTTON
#define USE_CLIMATE
#define USE_COVER
#define USE_DEEP_SLEEP
#define USE_FAN
#define USE_GRAPH
#define USE_HOMEASSISTANT_TIME
#define USE_JSON
#define USE_LIGHT
#define USE_LOCK
#define USE_LOGGER
#define USE_MDNS
#define USE_MEDIA_PLAYER
#define USE_MQTT
#define USE_NUMBER
#define USE_OTA
#define USE_OTA_PASSWORD
#define USE_OTA_STATE_CALLBACK
#define USE_OUTPUT
#define USE_POWER_SUPPLY
#define USE_QR_CODE
#define USE_SELECT
#define USE_SENSOR
#define USE_STATUS_LED
#define USE_SWITCH
#define USE_TEXT_SENSOR
#define USE_TIME
#define USE_TOUCHSCREEN
#define USE_UART_DEBUGGER
#define USE_WIFI

// Arduino-specific feature flags
#ifdef USE_ARDUINO
#define USE_CAPTIVE_PORTAL
#define USE_NEXTION_TFT_UPLOAD
#define USE_PROMETHEUS
#define USE_WEBSERVER
#define USE_WEBSERVER_PORT 80  // NOLINT
#define USE_WIFI_WPA2_EAP
#endif

// IDF-specific feature flags
#ifdef USE_ESP_IDF
#define USE_MQTT_IDF_ENQUEUE
#endif

// ESP32-specific feature flags
#ifdef USE_ESP32
#define USE_ESP32_BLE_CLIENT
#define USE_ESP32_BLE_SERVER
#define USE_ESP32_CAMERA
#define USE_IMPROV
#define USE_SOCKET_IMPL_BSD_SOCKETS
#define USE_WIFI_11KV_SUPPORT
#define USE_BLUETOOTH_PROXY
#define USE_VOICE_ASSISTANT
#define USE_MICROPHONE
#define USE_SPEAKER

#ifdef USE_ARDUINO
#define USE_ARDUINO_VERSION_CODE VERSION_CODE(2, 0, 5)
#define USE_ETHERNET
#endif

#ifdef USE_ESP_IDF
#define USE_ESP_IDF_VERSION_CODE VERSION_CODE(4, 4, 2)
#endif
#endif

// ESP8266-specific feature flags
#ifdef USE_ESP8266
#define USE_ADC_SENSOR_VCC
#define USE_ARDUINO_VERSION_CODE VERSION_CODE(3, 0, 2)
#define USE_ESP8266_PREFERENCES_FLASH
#define USE_HTTP_REQUEST_ESP8266_HTTPS
#define USE_SOCKET_IMPL_LWIP_TCP

#ifdef USE_LIBRETINY
#define USE_SOCKET_IMPL_LWIP_SOCKETS
#endif

// Dummy firmware payload for shelly_dimmer
#define USE_SHD_FIRMWARE_MAJOR_VERSION 56
#define USE_SHD_FIRMWARE_MINOR_VERSION 5
#define USE_SHD_FIRMWARE_DATA \
  {}

#endif

#ifdef USE_RP2040
#define USE_ARDUINO_VERSION_CODE VERSION_CODE(3, 3, 0)
#define USE_SOCKET_IMPL_LWIP_TCP
#endif

#ifdef USE_HOST
#define USE_SOCKET_IMPL_BSD_SOCKETS
#endif

// Disabled feature flags
// #define USE_BSEC  // Requires a library with proprietary license.

#define USE_DASHBOARD_IMPORT

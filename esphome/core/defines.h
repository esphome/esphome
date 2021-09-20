#pragma once

// This file is not used by the runtime, instead, a version is generated during
// compilation with only the relevant feature flags for the current build.
//
// This file is only used by static analyzers and IDEs.

// Informative flags
#define ESPHOME_BOARD "dummy_board"
#define ESPHOME_PROJECT_NAME "dummy project"
#define ESPHOME_PROJECT_VERSION "v2"

// Feature flags
#define USE_API
#define USE_BINARY_SENSOR
#ifdef USE_ARDUINO
#define USE_CAPTIVE_PORTAL
#endif
#define USE_CLIMATE
#define USE_COVER
#define USE_DEEP_SLEEP
#define USE_ESP8266_PREFERENCES_FLASH
#define USE_FAN
#define USE_GRAPH
#define USE_HOMEASSISTANT_TIME

#ifdef USE_ARDUINO
#define USE_JSON
#define USE_NEXTION_TFT_UPLOAD
#define USE_MQTT
#define USE_CAPTIVE_PORTAL
#endif  // USE_ARDUINO

#define USE_LIGHT
#define USE_LOGGER
#define USE_MDNS
#define USE_NUMBER
#define USE_OTA_STATE_CALLBACK
#define USE_POWER_SUPPLY
#define USE_PROMETHEUS
#define USE_SELECT
#define USE_SENSOR
#define USE_STATUS_LED
#define USE_SWITCH
#define USE_TEXT_SENSOR

#define USE_TIME
#define USE_WIFI
#ifdef USE_ARDUINO
#define USE_WIFI_WPA2_EAP
#endif

#ifdef USE_ESP32
#define USE_ESP32_BLE_SERVER
#define USE_ESP32_CAMERA

#ifdef USE_ARDUINO
#define USE_ETHERNET
#endif  // USE_ARDUINO

#define USE_IMPROV
#define USE_SOCKET_IMPL_BSD_SOCKETS
#endif  // USE_ESP32

#ifdef USE_ESP8266
#define USE_ADC_SENSOR_VCC
#define USE_SOCKET_IMPL_LWIP_TCP
#define USE_HTTP_REQUEST_ESP8266_HTTPS
#endif

#define USE_API_PLAINTEXT
#define USE_API_NOISE

// Disabled feature flags
//#define USE_BSEC  // Requires a library with proprietary license.
#define USE_TIME
#define USE_DEEP_SLEEP
#define ESPHOME_BOARD "dummy_board"
#define USE_MDNS
#define USE_API_NOISE
#define USE_API_PLAINTEXT

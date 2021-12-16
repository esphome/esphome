#pragma once

// This file is not used by the runtime, instead, a version is generated during
// compilation with only the relevant feature flags for the current build.
//
// This file is only used by static analyzers and IDEs.

// Informative flags
#define ESPHOME_BOARD "dummy_board"
#define ESPHOME_PROJECT_NAME "dummy project"
#define ESPHOME_PROJECT_VERSION "v2"
#define ESPHOME_VARIANT "ESP32"

// Feature flags
#define USE_API
#define USE_API_NOISE
#define USE_API_PLAINTEXT
#define USE_BINARY_SENSOR
#define USE_BUTTON
#define USE_CLIMATE
#define USE_COVER
#define USE_DEEP_SLEEP
#define USE_FAN
#define USE_GRAPH
#define USE_HOMEASSISTANT_TIME
#define USE_LIGHT
#define USE_LOGGER
#define USE_MDNS
#define USE_NUMBER
#define USE_OTA_PASSWORD
#define USE_OTA_STATE_CALLBACK
#define USE_POWER_SUPPLY
#define USE_SELECT
#define USE_SENSOR
#define USE_STATUS_LED
#define USE_SWITCH
#define USE_TEXT_SENSOR
#define USE_TIME
#define USE_UART_DEBUGGER
#define USE_WIFI

// Arduino-specific feature flags
#ifdef USE_ARDUINO
#define USE_CAPTIVE_PORTAL
#define USE_JSON
#define USE_NEXTION_TFT_UPLOAD
#define USE_MQTT
#define USE_PROMETHEUS
#define USE_WEBSERVER
#define USE_WIFI_WPA2_EAP
#define WEBSERVER_PORT 80  // NOLINT
#endif

// ESP32-specific feature flags
#ifdef USE_ESP32
#define USE_ESP32_BLE_SERVER
#define USE_ESP32_CAMERA
#define USE_ESP32_IGNORE_EFUSE_MAC_CRC
#define USE_IMPROV
#define USE_SOCKET_IMPL_BSD_SOCKETS

#ifdef USE_ARDUINO
#define USE_ETHERNET
#endif
#endif

// ESP8266-specific feature flags
#ifdef USE_ESP8266
#define USE_ADC_SENSOR_VCC
#define USE_ESP8266_PREFERENCES_FLASH
#define USE_HTTP_REQUEST_ESP8266_HTTPS
#define USE_SOCKET_IMPL_LWIP_TCP
#endif

// Disabled feature flags
//#define USE_BSEC  // Requires a library with proprietary license.

#define USE_DASHBOARD_IMPORT

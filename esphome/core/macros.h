#pragma once

#define VERSION_CODE(major, minor, patch) ((major) << 16 | (minor) << 8 | (patch))

#if defined(USE_ESP8266)

#include <core_version.h>
#if defined(ARDUINO_ESP8266_MAJOR) && defined(ARDUINO_ESP8266_MINOR) && defined(ARDUINO_ESP8266_REVISION)  // v3.0.1+
#define ARDUINO_VERSION_CODE VERSION_CODE(ARDUINO_ESP8266_MAJOR, ARDUINO_ESP8266_MINOR, ARDUINO_ESP8266_REVISION)
#elif ARDUINO_ESP8266_GIT_VER == 0xefb0341a  // version defines were screwed up in v3.0.0
#define ARDUINO_VERSION_CODE VERSION_CODE(3, 0, 0)
#elif defined(ARDUINO_ESP8266_RELEASE_2_7_4)
#define ARDUINO_VERSION_CODE VERSION_CODE(2, 7, 4)
#elif defined(ARDUINO_ESP8266_RELEASE_2_7_3)
#define ARDUINO_VERSION_CODE VERSION_CODE(2, 7, 3)
#elif defined(ARDUINO_ESP8266_RELEASE_2_7_2)
#define ARDUINO_VERSION_CODE VERSION_CODE(2, 7, 2)
#elif defined(ARDUINO_ESP8266_RELEASE_2_7_1)
#define ARDUINO_VERSION_CODE VERSION_CODE(2, 7, 1)
#elif defined(ARDUINO_ESP8266_RELEASE_2_7_0)
#define ARDUINO_VERSION_CODE VERSION_CODE(2, 7, 0)
#elif defined(ARDUINO_ESP8266_RELEASE_2_6_3)
#define ARDUINO_VERSION_CODE VERSION_CODE(2, 6, 3)
#elif defined(ARDUINO_ESP8266_RELEASE_2_6_2)
#define ARDUINO_VERSION_CODE VERSION_CODE(2, 6, 2)
#elif defined(ARDUINO_ESP8266_RELEASE_2_6_1)
#define ARDUINO_VERSION_CODE VERSION_CODE(2, 6, 1)
#elif defined(ARDUINO_ESP8266_RELEASE_2_5_2)
#define ARDUINO_VERSION_CODE VERSION_CODE(2, 5, 2)
#elif defined(ARDUINO_ESP8266_RELEASE_2_5_1)
#define ARDUINO_VERSION_CODE VERSION_CODE(2, 5, 1)
#elif defined(ARDUINO_ESP8266_RELEASE_2_5_0)
#define ARDUINO_VERSION_CODE VERSION_CODE(2, 5, 0)
#elif defined(ARDUINO_ESP8266_RELEASE_2_4_2)
#define ARDUINO_VERSION_CODE VERSION_CODE(2, 4, 2)
#elif defined(ARDUINO_ESP8266_RELEASE_2_4_1)
#define ARDUINO_VERSION_CODE VERSION_CODE(2, 4, 1)
#elif defined(ARDUINO_ESP8266_RELEASE_2_4_0)
#define ARDUINO_VERSION_CODE VERSION_CODE(2, 4, 0)
#elif defined(ARDUINO_ESP8266_RELEASE_2_3_0)
#define ARDUINO_VERSION_CODE VERSION_CODE(2, 3, 0)
#else
#warning "Could not determine Arduino framework version, update esphome/core/macros.h!"
#endif

#elif defined(USE_ESP32_FRAMEWORK_ARDUINO)

#if defined(IDF_VER)  // identifies v2, needed since v1 doesn't have the esp_arduino_version.h header
#include <esp_arduino_version.h>
#define ARDUINO_VERSION_CODE \
  VERSION_CODE(ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATH)
#else
#define ARDUINO_VERSION_CODE VERSION_CODE(1, 0, 0)  // there are no defines identifying minor/patch version
#endif

#endif

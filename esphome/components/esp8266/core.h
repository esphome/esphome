#pragma once

#ifdef USE_ESP8266

#include <cstdint>

extern const uint8_t ESPHOME_ESP8266_GPIO_INITIAL_MODE[16];
extern const uint8_t ESPHOME_ESP8266_GPIO_INITIAL_LEVEL[16];

namespace esphome {
namespace esp8266 {}  // namespace esp8266
}  // namespace esphome

#endif  // USE_ESP8266

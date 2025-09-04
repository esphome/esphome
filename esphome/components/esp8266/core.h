#pragma once

#ifdef USE_ESP8266

#include <cstdint>
#include "esphome/core/hal.h"

extern const uint8_t ESPHOME_ESP8266_GPIO_INITIAL_MODE[16] PROGMEM;
extern const uint8_t ESPHOME_ESP8266_GPIO_INITIAL_LEVEL[16] PROGMEM;

namespace esphome {
namespace esp8266 {}  // namespace esp8266
}  // namespace esphome

#endif  // USE_ESP8266
